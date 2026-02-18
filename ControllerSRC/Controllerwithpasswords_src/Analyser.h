//
//  Analyser.h
//
//  Created by John Grant on 22-07-2013, content imported from ConfigurePlatform.h.
//  Copyright 2011-2013 Nine Tiles. All rights reserved.

#import <Cocoa/Cocoa.h>
#import "AnalyserDoc.h"

@interface Analyser : NSDocument
{
		// attributes for output windows
	NSMutableParagraphStyle * style;
	NSDictionary * display_attributes;
	
		// The platform-independent C++ class <ConfigDoc> does all the 
		//		communication with the target
	AnalyserDoc pi_doc;

		// attributed string containing a newline character
	NSAttributedString * newline;	
	
		// paragraph type for console display
//	NSMutableParagraphStyle * hanging_para_style;

@public
	IBOutlet NSTextView * debug_view;
}

- (void)outputToConsole: (std::string) t;

@end

