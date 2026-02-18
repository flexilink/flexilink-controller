flexilink_protocol = Proto("AES51",  "Flexilink virtual link")

aes51_type = ProtoField.uint8("flexilink.aes51_type", "frame type", base.HEX)
fl_hdr = ProtoField.uint32("flexilink.fl_hdr", "IT packet header", base.HEX)
asn1 = ProtoField.uint8("flexilink.asn1_tag", "ASN.1", base.HEX)
ie_hdr = ProtoField.uint8("flexilink.ie_hdr", "IE", base.HEX)
sig_hdr = ProtoField.uint8("flexilink.sig_hdr", "sig msg type", base.HEX)

flexilink_protocol.fields = {aes_pcol, aes51_type, fl_hdr, asn1, ie_hdr, sig_hdr}

function flexilink_protocol.dissector(buffer, pinfo, tree)
  local length = buffer:len()
  if length < 6 or buffer(0,1):uint() ~= 0x02 then return end

  pinfo.cols.protocol = flexilink_protocol.name
  local offset -- into buffer when parsing IEs
  local ie_type
  local ie_length
  local ie_tree
  local n

  local subtree = tree:add(flexilink_protocol, buffer(), "AES51: ")
  local frame_type = buffer(1,1):uint()
  subtree:append_text(get_aes51_name(frame_type))
  if frame_type == 0x26 then
      -- Flexlink basic service packet: probably either signalling or management
      -- Ideally we'd pick up and remember the label values but that's not possible in general  (might for 
      --    instance not start collecting packets until after the route has been set up) so we'll just guess
      -- For management we expect the 3rd byte to be 04 (octet string tag) or 06 (OID tag); for signalling 
      --    we expect it to be 00 or 02 for a FindRoute, 00 for a cleardown
    subtree:append_text(string.format(" length %i label %i", bit.rshift(buffer(6,2):uint(), 3) + 1, bit.rshift(buffer(8,2):uint(), 3)))
    local msg_type = buffer(10,1):uint()
    local b2 = buffer(11,1):uint() -- second byte
    if length < 13 then
        -- up to 2 bytes payload: too short for signalling; must be management with no ASN.1
      subtree:append_text(string.format(": %s, serial %02X", get_mgt_msg_type(msg_type), b2))
      return
    end
    local b3 = buffer(12,1):uint()
    if length == 13 then
        -- might be console keypress
      if msg_type == 0x70 then subtree:append_text(string.format(": ConsoleData request, serial %02X: '%c'", b2, b3)) end
      return
    end
    local b4 = buffer(13,1):uint()
    if b3 == 6 or (b3 == 4 and b4 == 68) then
        -- management message
      subtree:append_text(string.format(": %s, serial %02X", get_mgt_msg_type(msg_type), b2))
        -- list the ASN.1 objects
      length = length - 12
      offset = 12;
      while length > 0 do
        ie_type = buffer(offset, 1):uint()
        ie_tree = subtree:add(asn1, ie_type, get_asn1_tag(ie_type))
        ie_length = buffer(offset + 1, 1):uint()
        offset = offset + 2
        length = length - 2
        if ie_length > 0x80 then
            -- multibyte format for the length
          n = ie_length - 0x80 -- number of bytes of length
          ie_length = buffer(offset, n):uint()
          offset = offset + n
          length = length - n
        end
        length = length - ie_length
        if ie_type == 6 then
            -- OID
          n = buffer(offset, 1):uint()
          ie_tree:append_text(string.format(": %i.%i", n/40, n%40))
          while ie_length > 1 do
            offset = offset + 1
            ie_length = ie_length - 1
            n = buffer(offset, 1):uint()
            while buffer(offset, 1):uint() >= 0x80 do
              offset = offset + 1
              ie_length = ie_length - 1
              n = ((n - 0x80) * 128) + buffer(offset, 1):uint()
            end
            ie_tree:append_text(string.format(".%i", n))
          end
          offset = offset + 1
        elseif ie_type == 2 then
            -- integer
          ie_tree:append_text(string.format(": %i", buffer(offset, ie_length):int()))
          offset = offset + ie_length
        else
          ie_tree:append_text(":")
          b3 = (ie_type == 4)
          b4 = offset
          while ie_length > 0 do
            n = buffer(offset, 1):uint()
            ie_tree:append_text(string.format(" %02X", n))
            if n > 126 or n < 32 then b3 = false end
            offset = offset + 1
            ie_length = ie_length - 1
          end
          if b3 and offset ~= b4 then
              -- have an octet string that is all ASCII characters
            ie_tree:append_text(" \"" .. buffer(b4, offset - b4):string() .. "\"")
          end
        end
      end
      return
    end

    if msg_type == 0x70 then
        -- assume it's console data
      subtree:append_text(string.format(": ConsoleData request, serial %02X: \"", b2))
      subtree:append_text(buffer(12, length - 12):string() .. "\"")
      return
    end

    if b3 ~= 0 and b3 ~= 2 then return end -- expected values for RouteId and SerialNumber fixed IEs

      -- here if a signalling message; <b2> is length of fixed part
    subtree:append_text(": " .. get_sig_msg_type(msg_type))
    list_sig_msg(buffer, 12, b2, length - (b2 + 12), subtree)
    return
  end

  if frame_type < 0x80 or frame_type > 0x84 or length < 8 then return end

    -- here if link negotiation
  length = length - 6
  offset = 6
  repeat
    ie_type = buffer(offset, 1):uint()
    ie_tree = subtree:add(ie_hdr, ie_type)
    if ie_type == 0 then return end
      -- consume the IE type
    offset = offset + 1
    length = length - 1
    ie_length = buffer(offset, 1):uint() + 1
    if length < ie_length then return end
    length = length - ie_length
      -- now add <ie_length> hex bytes
    repeat
      ie_tree:append_text(string.format(" %02X", buffer(offset, 1):uint()))
      offset = offset + 1
      ie_length = ie_length - 1
    until ie_length == 0
  until length <= 0
end

-- create a subtree listing the variable part of a signalling message or IE
-- <buffer> contains <f_len> bytes of fixed part starting at <offset> followed by <v_len> bytes of variable part
-- <indent> is a string to output at the start of each line
-- appends the fixed part to <tree> in hex and adds the IEs in the variable part as subtrees
function list_sig_msg(buffer, offset, f_len, v_len, tree)
  while f_len > 0 do
    tree:append_text(string.format(" %02X", buffer(offset, 1):uint()))
    offset = offset + 1
    f_len = f_len - 1
  end
  while v_len > 2 do
      -- list embedded IE starting at <offset>
    local ie_len = buffer(offset + 1, 2):uint()
    if ie_len > (v_len - 3) then
      tree:append_text(" [not a valid signalling message]")
      return
    end
    local ie_type = buffer(offset, 1):uint()
    ie_tree = tree:add(ie_hdr, ie_type, get_ie_type(bit.band(ie_type, 0x7F)))
    if ie_type > 0x7F then
        -- have a variable part
      local ie_f_len = buffer(offset + 3, 1):uint()
      list_sig_msg(buffer, offset + 4, ie_f_len, ie_len - (ie_f_len + 1), ie_tree)
    else list_sig_msg(buffer, offset + 3, ie_len, 0, ie_tree) end
    offset = offset + ie_len + 3
    v_len = v_len - (ie_len + 3)
  end
end

function get_aes51_name(type)
  if type == 0x26 then return "Flexilink packet" end
  if type == 0x27 then return "Link timing" end
  if type == 0x80 then return "Link request" end
  if type == 0x81 then return "Link accept" end
  if type == 0x82 then return "Link reject" end
  if type == 0x83 then return "Link confirm" end
  if type == 0x84 then return "Link keep-alive" end
  return string.format("type %X", type)
end

function get_mgt_msg_type(type)
  local code = bit.band(type, 0x70)
  local name
  if code == 0 then name = "Get"
  elseif code == 0x10 then name = "GetNext"
  elseif code == 0x20 then name = "Status"
  elseif code == 0x30 then name = "Set"
  elseif code == 0x40 then name = "NvSet"
  elseif code == 0x50 then name = "StringReq"
  elseif code == 0x70 then name = "ConsoleData"
  else name = string.format("[type %d]", bit.rshift(code, 4)) end
  if bit.band(type, 0x80) == 0 then name = name .. " request"
  else name = name .. " response" end
  code = bit.band(type, 0x0F)
  if code == 0 then return name end
  return name .. string.format(", status %i", code)
end

function get_asn1_tag(code)
  if code == 2 then return "INTEGER" end
  if code == 4 then return "OCTETSTRING" end
  if code == 5 then return "NULL" end
  if code == 6 then return "OID" end
  return string.format("ASN.1 tag %X", code)
end

-- <code> is the "type" value without the V bit
function get_ie_type(code)
  if code == 3 then return "CalledAddress" end
  if code == 4 then return "FlowDescriptor" end
  if code == 5 then return "DataType" end
  if code == 6 then return "StartTime" end
  if code == 7 then return "EndTime" end
  if code == 8 then return "Importance" end
  if code == 9 then return "ServiceName" end
  if code == 10 then return "SourceName" end
  if code == 11 then return "DestinationName" end
  if code == 12 then return "PrivilegeLevel" end
  if code == 13 then return "Password" end
  if code == 14 then return "Charge" end
  if code == 15 then return "CallingAddress" end
  if code == 16 then return "RouteMetric" end
  if code == 17 then return "SyncParams" end
  if code == 18 then return "AsyncParams" end
  if code == 19 then return "SyncAlloc" end
  if code == 20 then return "AsyncAlloc" end
  if code == 21 then return "Delay" end
  if code == 22 then return "McastRoute" end
  if code == 23 then return "Cause" end
  if code == 24 then return "Route" end
  if code == 25 then return "Alternatives" end
  if code == 26 then return "Group" end
  if code == 27 then return "InterimOffer" end
  if code == 28 then return "PathMTU" end
  if code == 29 then return "DestCount" end
  if code == 20 then return "DestSelection" end
  if code == 31 then return "UserData" end
  return string.format("IE tag %X", code)
end

function get_sig_msg_type(type)
  local code = bit.band(type, 0x1F)
  local name
  if code == 8 then name = "FindRoute"
  elseif code == 9 then name = "ClearDown"
  elseif code == 10 then name = "AddFlow"
  elseif code == 11 then name = "NetworkData"
  elseif code == 12 then name = "EndToEndData"
  elseif code == 13 then name = "AsyncSetup"
  elseif code == 6 then name = "LinkInfo"
  elseif code == 7 then name = "Syncnfo"
  else return string.format("type %02X", type) end
  if bit.band(type, 0x80) == 0x80 then name = "ack " .. name end
  code = bit.band(type, 0x60)
  if code == 0 then return name .. " request" end
  if code == 0x20 then return name .. " response" end
  if code == 0x40 then return name .. " confirmation" end
  return name .. " completion"
end

local udp_port = DissectorTable.get("udp.port")
udp_port:add(35037, flexilink_protocol)
