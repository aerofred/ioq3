#include "ios_numpad.h"
#include "../qcommon/qcommon.h"

#import <UIKit/UIKit.h>

#define IOS_NUMPAD_MAX_LEN 64
#define IOS_NUMPAD_TAG_BACKSPACE 10
#define IOS_NUMPAD_TAG_DOT 11
#define IOS_NUMPAD_TAG_CONNECT 12

static UIWindow *iosNumpadWindow = nil;
static UILabel *iosNumpadDisplay = nil;
static UILabel *iosNumpadStatus = nil;
static UIView *iosNumpadPanel = nil;
static NSTimer *iosNumpadStatusTimer = nil;
static char iosNumpadBuffer[IOS_NUMPAD_MAX_LEN + 1];

static void IOS_Numpad_OnMain( void (^block)( void ) )
{
	if ( [NSThread isMainThread] )
		block();
	else
		dispatch_async( dispatch_get_main_queue(), block );
}

static UIWindowScene *IOS_Numpad_ActiveScene( void )
{
	for ( UIScene *scene in UIApplication.sharedApplication.connectedScenes )
	{
		if ( [scene isKindOfClass:[UIWindowScene class]] &&
			scene.activationState == UISceneActivationStateForegroundActive )
			return (UIWindowScene *)scene;
	}

	for ( UIScene *scene in UIApplication.sharedApplication.connectedScenes )
	{
		if ( [scene isKindOfClass:[UIWindowScene class]] )
			return (UIWindowScene *)scene;
	}

	return nil;
}

static void IOS_Numpad_SyncCvar( void )
{
	Cvar_Set( "ui_specifyAddress", iosNumpadBuffer );
}

static void IOS_Numpad_UpdateDisplay( void )
{
	if ( iosNumpadDisplay )
		iosNumpadDisplay.text = [NSString stringWithUTF8String:iosNumpadBuffer];
}

static void IOS_Numpad_UpdateStatus( void )
{
	char buf[256];

	if ( !iosNumpadStatus )
		return;

	buf[0] = '\0';
	Cvar_VariableStringBuffer( "ui_lan_debug", buf, sizeof( buf ) );
	if ( !buf[0] )
		Q_strncpyz( buf, "LAN: (ouvre la liste des serveurs pour scanner)", sizeof( buf ) );

	iosNumpadStatus.text = [NSString stringWithUTF8String:buf];
}

static void IOS_Numpad_AppendDigit( unichar digit )
{
	size_t len;

	len = strlen( iosNumpadBuffer );
	if ( len >= IOS_NUMPAD_MAX_LEN )
		return;

	iosNumpadBuffer[len] = (char)digit;
	iosNumpadBuffer[len + 1] = '\0';
	IOS_Numpad_SyncCvar();
	IOS_Numpad_UpdateDisplay();
}

static void IOS_Numpad_Backspace( void )
{
	size_t len;

	len = strlen( iosNumpadBuffer );
	if ( len == 0 )
		return;

	iosNumpadBuffer[len - 1] = '\0';
	IOS_Numpad_SyncCvar();
	IOS_Numpad_UpdateDisplay();
}

static void IOS_Numpad_Connect( void )
{
	char cmd[IOS_NUMPAD_MAX_LEN + 32];

	if ( strlen( iosNumpadBuffer ) == 0 )
		return;

	IOS_Numpad_SyncCvar();

	Com_sprintf( cmd, sizeof( cmd ), "connect %s\n", iosNumpadBuffer );
	Cbuf_AddText( cmd );

	IOS_Numpad_Hide();
}

/*
============================================================================
Overlay window that lets touches outside the keypad panel fall through to
the game (SDL) window, so the player keeps mouse/menu control while typing.
============================================================================
*/
@interface SGNumpadWindow : UIWindow
@end

@implementation SGNumpadWindow

- (UIView *)hitTest:(CGPoint)point withEvent:(UIEvent *)event
{
	UIView *hit = [super hitTest:point withEvent:event];

	// Only the keypad panel (and its subviews) should capture touches.
	// Everything else passes through to the window beneath us.
	if ( hit == self || hit == self.rootViewController.view )
		return nil;

	return hit;
}

@end

@interface SGNumpadViewController : UIViewController
@end

@implementation SGNumpadViewController

- (void)loadView
{
	self.view = [[UIView alloc] initWithFrame:UIScreen.mainScreen.bounds];
	self.view.backgroundColor = UIColor.clearColor;
}

- (void)numpadKeyTapped:( UIButton * )sender
{
	if ( sender.tag == IOS_NUMPAD_TAG_BACKSPACE )
		IOS_Numpad_Backspace();
	else if ( sender.tag == IOS_NUMPAD_TAG_DOT )
		IOS_Numpad_AppendDigit( '.' );
	else if ( sender.tag == IOS_NUMPAD_TAG_CONNECT )
		IOS_Numpad_Connect();
	else
		IOS_Numpad_AppendDigit( (unichar)( '0' + sender.tag ) );
}

- (BOOL)prefersStatusBarHidden
{
	return YES;
}

@end

static UIButton *IOS_Numpad_MakeKey( SGNumpadViewController *vc, NSString *title,
	int tag, CGRect frame, UIColor *fill )
{
	UIButton *button;
	UIColor *gold = [UIColor colorWithRed:1.0 green:0.75 blue:0.0 alpha:1.0];

	button = [UIButton buttonWithType:UIButtonTypeCustom];
	button.frame = frame;
	button.tag = tag;
	button.backgroundColor = fill ? fill : [UIColor colorWithWhite:0.12 alpha:0.94];
	button.layer.cornerRadius = 8.0;
	button.layer.borderWidth = 1.0;
	button.layer.borderColor = gold.CGColor;
	[button setTitle:title forState:UIControlStateNormal];
	[button setTitleColor:[UIColor whiteColor] forState:UIControlStateNormal];
	button.titleLabel.font = [UIFont boldSystemFontOfSize:24.0];
	[button addTarget:vc action:@selector(numpadKeyTapped:) forControlEvents:UIControlEventTouchUpInside];
	return button;
}

static void IOS_Numpad_BuildKeys( SGNumpadViewController *vc, UIView *panel, CGRect bounds )
{
	CGFloat margin = 10.0;
	CGFloat gap = 8.0;
	CGFloat statusH = 20.0;
	CGFloat displayH = 40.0;
	CGFloat connectH = 40.0;
	CGFloat cols = 3.0;
	CGFloat rowCount = 4.0;
	CGFloat keyAreaTop = margin + statusH + displayH + gap * 2.0;
	CGFloat keyW = ( bounds.size.width - margin * 2.0 - gap * ( cols - 1.0 ) ) / cols;
	CGFloat keyH = ( bounds.size.height - keyAreaTop - connectH - margin - gap * rowCount ) / rowCount;
	CGFloat x0 = margin;
	CGFloat y0 = keyAreaTop;
	UIColor *connectFill = [UIColor colorWithRed:0.15 green:0.45 blue:0.15 alpha:0.96];
	NSArray<NSString *> *row1 = @[ @"1", @"2", @"3" ];
	NSArray<NSString *> *row2 = @[ @"4", @"5", @"6" ];
	NSArray<NSString *> *row3 = @[ @"7", @"8", @"9" ];
	NSArray<NSString *> *row4 = @[ @"\u232b", @"0", @"." ];
	NSArray<NSArray<NSString *> *> *keyRows = @[ row1, row2, row3, row4 ];
	NSUInteger r;
	UIButton *connect;

	for ( r = 0; r < keyRows.count; r++ )
	{
		NSArray<NSString *> *row = keyRows[r];
		NSUInteger c;

		for ( c = 0; c < row.count; c++ )
		{
			NSString *title = row[c];
			int tag;
			CGRect frame = CGRectMake(
				x0 + (CGFloat)c * ( keyW + gap ),
				y0 + (CGFloat)r * ( keyH + gap ),
				keyW,
				keyH );

			if ( [title isEqualToString:@"\u232b"] )
				tag = IOS_NUMPAD_TAG_BACKSPACE;
			else if ( [title isEqualToString:@"."] )
				tag = IOS_NUMPAD_TAG_DOT;
			else
				tag = (int)( [title characterAtIndex:0] - '0' );

			[panel addSubview:IOS_Numpad_MakeKey( vc, title, tag, frame, nil )];
		}
	}

	connect = IOS_Numpad_MakeKey( vc, @"Connecter", IOS_NUMPAD_TAG_CONNECT,
		CGRectMake( margin, bounds.size.height - connectH - margin,
			bounds.size.width - margin * 2.0, connectH ),
		connectFill );
	connect.titleLabel.font = [UIFont boldSystemFontOfSize:18.0];
	[panel addSubview:connect];
}

static void IOS_Numpad_EnsureWindow( void )
{
	UIWindowScene *scene;
	SGNumpadViewController *vc;
	UIView *panel;
	CGRect screenBounds;
	CGFloat panelH;
	UIColor *gold;

	if ( iosNumpadWindow )
		return;

	scene = IOS_Numpad_ActiveScene();
	screenBounds = UIScreen.mainScreen.bounds;
	panelH = MIN( 400.0, screenBounds.size.height * 0.52 );

	if ( scene )
		iosNumpadWindow = [[SGNumpadWindow alloc] initWithWindowScene:scene];
	else
		iosNumpadWindow = [[SGNumpadWindow alloc] initWithFrame:screenBounds];

	iosNumpadWindow.windowLevel = UIWindowLevelAlert + 30.0;
	iosNumpadWindow.backgroundColor = UIColor.clearColor;

	vc = [[SGNumpadViewController alloc] init];
	iosNumpadWindow.rootViewController = vc;

	panel = [[UIView alloc] initWithFrame:CGRectMake(
		0.0, screenBounds.size.height - panelH, screenBounds.size.width, panelH )];
	panel.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleTopMargin;
	panel.backgroundColor = [UIColor colorWithWhite:0.05 alpha:0.92];
	gold = [UIColor colorWithRed:1.0 green:0.75 blue:0.0 alpha:1.0];
	panel.layer.borderColor = gold.CGColor;
	panel.layer.borderWidth = 1.0;
	iosNumpadPanel = panel;

	iosNumpadStatus = [[UILabel alloc] initWithFrame:CGRectMake( 10.0, 6.0, panel.bounds.size.width - 20.0, 18.0 )];
	iosNumpadStatus.autoresizingMask = UIViewAutoresizingFlexibleWidth;
	iosNumpadStatus.textColor = [UIColor colorWithRed:1.0 green:0.75 blue:0.0 alpha:1.0];
	iosNumpadStatus.font = [UIFont systemFontOfSize:11.0];
	iosNumpadStatus.textAlignment = NSTextAlignmentCenter;
	iosNumpadStatus.adjustsFontSizeToFitWidth = YES;
	iosNumpadStatus.minimumScaleFactor = 0.6;
	iosNumpadStatus.text = @"";

	iosNumpadDisplay = [[UILabel alloc] initWithFrame:CGRectMake( 10.0, 28.0, panel.bounds.size.width - 20.0, 34.0 )];
	iosNumpadDisplay.autoresizingMask = UIViewAutoresizingFlexibleWidth;
	iosNumpadDisplay.backgroundColor = [UIColor colorWithWhite:0.0 alpha:0.55];
	iosNumpadDisplay.textColor = [UIColor whiteColor];
	iosNumpadDisplay.font = [UIFont monospacedDigitSystemFontOfSize:22.0 weight:UIFontWeightMedium];
	iosNumpadDisplay.textAlignment = NSTextAlignmentCenter;
	iosNumpadDisplay.layer.cornerRadius = 6.0;
	iosNumpadDisplay.layer.masksToBounds = YES;
	iosNumpadDisplay.text = @"";

	[panel addSubview:iosNumpadStatus];
	[panel addSubview:iosNumpadDisplay];
	IOS_Numpad_BuildKeys( vc, panel, panel.bounds );
	[vc.view addSubview:panel];
}

void IOS_Numpad_Show( void )
{
	IOS_Numpad_OnMain( ^{
		IOS_Numpad_EnsureWindow();
		Cvar_VariableStringBuffer( "ui_specifyAddress", iosNumpadBuffer, sizeof( iosNumpadBuffer ) );
		IOS_Numpad_UpdateDisplay();
		IOS_Numpad_UpdateStatus();
		iosNumpadWindow.hidden = NO;
		[iosNumpadWindow makeKeyAndVisible];

		if ( !iosNumpadStatusTimer )
		{
			iosNumpadStatusTimer = [NSTimer scheduledTimerWithTimeInterval:1.0
				repeats:YES
				block:^( NSTimer *timer ) {
					IOS_Numpad_UpdateStatus();
				}];
		}
	} );
}

void IOS_Numpad_Hide( void )
{
	IOS_Numpad_OnMain( ^{
		if ( iosNumpadStatusTimer )
		{
			[iosNumpadStatusTimer invalidate];
			iosNumpadStatusTimer = nil;
		}

		if ( iosNumpadWindow )
			iosNumpadWindow.hidden = YES;
	} );
}

qboolean IOS_Numpad_IsVisible( void )
{
	return ( iosNumpadWindow && !iosNumpadWindow.hidden ) ? qtrue : qfalse;
}
