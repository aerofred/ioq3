#include "ios_layer.h"
#include "../client/cl_touch.h"

#import <UIKit/UIKit.h>

static iosLayout_t iosLayout = { 0, 0, 0, 0, 0, 0, 1 };
static qboolean iosOverlayVisible = qtrue;
static volatile qboolean iosAppActive = qtrue;
static qboolean iosLifecycleObserversInstalled = qfalse;

typedef struct iosTouchOverlayState_s
{
	qboolean visible;
	float opacity;
	float moveX, moveY, moveRadius;
	float jumpX, jumpY, jumpRadius;
	qboolean jumpActive;
	float crouchX, crouchY, crouchRadius;
	qboolean crouchActive;
	float useX, useY, useRadius;
	qboolean useActive;
	float weaponX, weaponY, weaponRadius;
	float menuX, menuY, menuRadius;
} iosTouchOverlayState_t;

typedef struct iosTouchOverlayState2_s
{
	float configX, configY, configRadius;
	qboolean configActive;
	qboolean editMode;
	float sliderX, sliderY, sliderW;
	float sliderValue;
} iosTouchOverlayState2_t;

static iosTouchOverlayState_t iosTouchOverlay;
static iosTouchOverlayState2_t iosTouchOverlay2;

@interface IOQ3TouchOverlayView : UIView
@end

@interface IOQ3TouchOverlayController : UIViewController
@end

static UIWindow *iosTouchWindow = nil;
static IOQ3TouchOverlayView *iosTouchView = nil;

static void IOS_OnMainAsync( void (^block)( void ) )
{
	if( [NSThread isMainThread] )
		block();
	else
		dispatch_async( dispatch_get_main_queue(), block );
}

static CGFloat IOS_PointScale( void )
{
	return iosLayout.scale > 0.0f ? (CGFloat)iosLayout.scale : UIScreen.mainScreen.scale;
}

static CGRect IOS_RectFromPixelCenter( float x, float y, float radius, CGFloat factor )
{
	CGFloat scale = IOS_PointScale();
	CGFloat pr = (CGFloat)radius / scale;
	CGFloat px = (CGFloat)x / scale;
	CGFloat py = (CGFloat)y / scale;

	return CGRectMake( px - pr * factor, py - pr * factor, pr * factor * 2.0, pr * factor * 2.0 );
}

static void IOS_HideSystemChrome( void )
{
	IOS_OnMainAsync( ^{
		UIApplication *app = UIApplication.sharedApplication;
		if( [app respondsToSelector:@selector(setStatusBarHidden:withAnimation:)] )
			[app setStatusBarHidden:YES withAnimation:UIStatusBarAnimationNone];

		for( UIWindow *window in app.windows )
		{
			[window.rootViewController setNeedsStatusBarAppearanceUpdate];
			if( [window.rootViewController respondsToSelector:@selector(setNeedsUpdateOfHomeIndicatorAutoHidden)] )
				[window.rootViewController setNeedsUpdateOfHomeIndicatorAutoHidden];
		}
	} );
}

static UIWindowScene *IOS_ActiveWindowScene( void )
{
	for( UIScene *scene in UIApplication.sharedApplication.connectedScenes )
	{
		if( [scene isKindOfClass:[UIWindowScene class]] &&
			scene.activationState == UISceneActivationStateForegroundActive )
			return (UIWindowScene *)scene;
	}

	for( UIScene *scene in UIApplication.sharedApplication.connectedScenes )
	{
		if( [scene isKindOfClass:[UIWindowScene class]] )
			return (UIWindowScene *)scene;
	}

	return nil;
}

static void IOS_UpdateLayoutFromView( UIView *view )
{
	CGRect bounds;
	CGFloat scale;

	if( !view )
		return;

	bounds = view.bounds;
	scale = view.window.screen.scale > 0.0 ? view.window.screen.scale : UIScreen.mainScreen.scale;
	iosLayout.width = (float)bounds.size.width;
	iosLayout.height = (float)bounds.size.height;
	iosLayout.scale = (float)scale;

	if( @available(iOS 11.0, *) )
	{
		UIEdgeInsets insets = view.safeAreaInsets;
		iosLayout.safeLeft = (float)insets.left;
		iosLayout.safeTop = (float)insets.top;
		iosLayout.safeRight = (float)insets.right;
		iosLayout.safeBottom = (float)insets.bottom;
	}
}

static void IOS_EnsureTouchOverlay( void )
{
	IOS_OnMainAsync( ^{
		UIWindowScene *scene;
		IOQ3TouchOverlayController *vc;

		if( iosTouchWindow && iosTouchView )
		{
			IOS_HideSystemChrome();
			return;
		}

		scene = IOS_ActiveWindowScene();
		if( scene )
			iosTouchWindow = [[UIWindow alloc] initWithWindowScene:scene];
		else
			iosTouchWindow = [[UIWindow alloc] initWithFrame:UIScreen.mainScreen.bounds];

		iosTouchWindow.backgroundColor = UIColor.clearColor;
		iosTouchWindow.opaque = NO;
		iosTouchWindow.windowLevel = UIWindowLevelStatusBar + 2.0;

		vc = [[IOQ3TouchOverlayController alloc] init];
		iosTouchView = [[IOQ3TouchOverlayView alloc] initWithFrame:UIScreen.mainScreen.bounds];
		iosTouchView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
		iosTouchView.backgroundColor = UIColor.clearColor;
		iosTouchView.opaque = NO;
		iosTouchView.multipleTouchEnabled = YES;
		iosTouchView.userInteractionEnabled = YES;
		vc.view = iosTouchView;
		iosTouchWindow.rootViewController = vc;
		[iosTouchWindow setHidden:NO];
		IOS_UpdateLayoutFromView( iosTouchView );
		IOS_HideSystemChrome();
	} );
}

@implementation IOQ3TouchOverlayController
- (BOOL)prefersStatusBarHidden { return YES; }
- (UIStatusBarAnimation)preferredStatusBarUpdateAnimation { return UIStatusBarAnimationNone; }
- (BOOL)prefersHomeIndicatorAutoHidden { return YES; }
@end

@implementation IOQ3TouchOverlayView
- (void)layoutSubviews
{
	[super layoutSubviews];
	IOS_UpdateLayoutFromView( self );
	IOS_HideSystemChrome();
}

- (void)drawRect:(CGRect)rect
{
	CGContextRef ctx = UIGraphicsGetCurrentContext();
	CGFloat alpha;
	(void)rect;

	if( !ctx || !iosOverlayVisible || !iosTouchOverlay.visible )
		return;

	alpha = iosTouchOverlay.opacity;
	if( alpha < 0.05 )
		alpha = 0.05;
	if( alpha > 0.85 )
		alpha = 0.85;

	CGContextSetLineWidth( ctx, 6.0 );

	void (^drawButton)( float, float, float, NSString *, UIColor *, BOOL ) =
		^( float x, float y, float radius, NSString *label, UIColor *color, BOOL active ) {
			CGRect innerRect = IOS_RectFromPixelCenter( x, y, radius, 0.58 );
			CGRect outerRect = IOS_RectFromPixelCenter( x, y, radius, 1.0 );
			CGFloat a = active ? MIN( alpha + 0.22, 0.92 ) : alpha;

			CGContextSetFillColorWithColor( ctx, [color colorWithAlphaComponent:a].CGColor );
			CGContextFillEllipseInRect( ctx, innerRect );
			CGContextSetStrokeColorWithColor( ctx, [UIColor colorWithWhite:1.0 alpha:( active ? 0.72 : 0.42 )].CGColor );
			CGContextStrokeEllipseInRect( ctx, outerRect );

			NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];
			style.alignment = NSTextAlignmentCenter;
			NSDictionary *attrs = @{
				NSFontAttributeName: [UIFont systemFontOfSize:16 weight:UIFontWeightSemibold],
				NSForegroundColorAttributeName: [UIColor colorWithWhite:1.0 alpha:0.92],
				NSParagraphStyleAttributeName: style
			};
			CGRect textRect = CGRectInset( outerRect, 6.0, radius * 0.68 );
			[label drawInRect:textRect withAttributes:attrs];
		};

	if( iosTouchOverlay.moveRadius > 0.0f )
	{
		CGContextSetStrokeColorWithColor( ctx, [UIColor colorWithWhite:1.0 alpha:0.36].CGColor );
		CGContextStrokeEllipseInRect( ctx, IOS_RectFromPixelCenter( iosTouchOverlay.moveX, iosTouchOverlay.moveY, iosTouchOverlay.moveRadius, 1.0 ) );
		CGContextSetFillColorWithColor( ctx, [UIColor colorWithWhite:1.0 alpha:0.16].CGColor );
		CGContextFillEllipseInRect( ctx, IOS_RectFromPixelCenter( iosTouchOverlay.moveX, iosTouchOverlay.moveY, iosTouchOverlay.moveRadius * 0.42f, 1.0 ) );
	}

	drawButton( iosTouchOverlay.jumpX, iosTouchOverlay.jumpY, iosTouchOverlay.jumpRadius, @"JMP", [UIColor colorWithRed:0.16 green:0.58 blue:0.98 alpha:1.0], iosTouchOverlay.jumpActive );
	drawButton( iosTouchOverlay.crouchX, iosTouchOverlay.crouchY, iosTouchOverlay.crouchRadius, @"DUCK", [UIColor colorWithRed:0.98 green:0.42 blue:0.22 alpha:1.0], iosTouchOverlay.crouchActive );
	drawButton( iosTouchOverlay.useX, iosTouchOverlay.useY, iosTouchOverlay.useRadius, @"USE", [UIColor colorWithRed:0.30 green:0.78 blue:0.38 alpha:1.0], iosTouchOverlay.useActive );
	drawButton( iosTouchOverlay.weaponX, iosTouchOverlay.weaponY, iosTouchOverlay.weaponRadius, @"NEXT", [UIColor colorWithRed:0.95 green:0.74 blue:0.18 alpha:1.0], NO );
	drawButton( iosTouchOverlay.menuX, iosTouchOverlay.menuY, iosTouchOverlay.menuRadius, @"MENU", [UIColor colorWithRed:0.62 green:0.54 blue:0.95 alpha:1.0], NO );
	drawButton( iosTouchOverlay2.configX, iosTouchOverlay2.configY, iosTouchOverlay2.configRadius, iosTouchOverlay2.editMode ? @"DONE" : @"CFG", [UIColor colorWithRed:0.96 green:0.52 blue:0.82 alpha:1.0], iosTouchOverlay2.configActive );

	if( iosTouchOverlay2.editMode && iosTouchOverlay2.sliderW > 0.0f )
	{
		CGFloat scale = IOS_PointScale();
		CGFloat x = iosTouchOverlay2.sliderX / scale;
		CGFloat y = iosTouchOverlay2.sliderY / scale;
		CGFloat w = iosTouchOverlay2.sliderW / scale;
		CGFloat knobX = x + w * iosTouchOverlay2.sliderValue;

		CGContextSetStrokeColorWithColor( ctx, [UIColor colorWithWhite:1.0 alpha:0.42].CGColor );
		CGContextMoveToPoint( ctx, x, y );
		CGContextAddLineToPoint( ctx, x + w, y );
		CGContextStrokePath( ctx );

		CGContextSetFillColorWithColor( ctx, [UIColor colorWithRed:1.0 green:1.0 blue:1.0 alpha:0.92].CGColor );
		CGContextFillEllipseInRect( ctx, CGRectMake( knobX - 10.0, y - 10.0, 20.0, 20.0 ) );
	}
}

- (void)forwardTouches:(NSSet<UITouch *> *)touches down:(BOOL)down motion:(BOOL)motion
{
	CGRect bounds = self.bounds;

	for( UITouch *touch in touches )
	{
		CGPoint p = [touch locationInView:self];
		float nx = bounds.size.width > 0.0 ? (float)( p.x / bounds.size.width ) : 0.0f;
		float ny = bounds.size.height > 0.0 ? (float)( p.y / bounds.size.height ) : 0.0f;

		IN_TouchFinger( (long long)(uintptr_t)touch, nx, ny, down ? qtrue : qfalse, motion ? qtrue : qfalse );
	}
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	[self forwardTouches:touches down:YES motion:NO];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	[self forwardTouches:touches down:YES motion:YES];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	[self forwardTouches:touches down:NO motion:NO];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
	(void)event;
	[self forwardTouches:touches down:NO motion:NO];
}

@end

void IOS_Layer_SetActive( qboolean active )
{
	iosAppActive = active;
}

qboolean IOS_Layer_IsActive( void )
{
	return iosAppActive;
}

static void IOS_InstallLifecycleObservers( void )
{
	if( iosLifecycleObserversInstalled )
		return;

	iosLifecycleObserversInstalled = qtrue;
	IOS_OnMainAsync( ^{
		NSNotificationCenter *center = NSNotificationCenter.defaultCenter;
		NSOperationQueue *queue = NSOperationQueue.mainQueue;

		[center addObserverForName:UIApplicationWillResignActiveNotification object:nil queue:queue
			usingBlock:^( NSNotification *note ) {
				(void)note;
				IOS_Layer_SetActive( qfalse );
			}];
		[center addObserverForName:UIApplicationDidEnterBackgroundNotification object:nil queue:queue
			usingBlock:^( NSNotification *note ) {
				(void)note;
				IOS_Layer_SetActive( qfalse );
			}];
		[center addObserverForName:UIApplicationWillEnterForegroundNotification object:nil queue:queue
			usingBlock:^( NSNotification *note ) {
				(void)note;
				IOS_Layer_SetActive( qtrue );
			}];
		[center addObserverForName:UIApplicationDidBecomeActiveNotification object:nil queue:queue
			usingBlock:^( NSNotification *note ) {
				(void)note;
				IOS_Layer_SetActive( qtrue );
				IOS_HideSystemChrome();
			}];
	} );
}

void IOS_Layer_Init( void )
{
	IOS_InstallLifecycleObservers();
	IOS_EnsureTouchOverlay();
}

void IOS_Layer_Shutdown( void )
{
	IOS_OnMainAsync( ^{
		if( iosTouchWindow )
		{
			iosTouchWindow.hidden = YES;
			iosTouchWindow = nil;
			iosTouchView = nil;
		}
	} );
}

void IOS_Layer_Tick( void )
{
	IOS_OnMainAsync( ^{
		if( iosTouchView )
			[iosTouchView setNeedsDisplay];
	} );
}

void IOS_Layer_SyncScreen( int width, int height, float scale )
{
	if( width > 0 )
		iosLayout.width = width / ( scale > 0.0f ? scale : 1.0f );
	if( height > 0 )
		iosLayout.height = height / ( scale > 0.0f ? scale : 1.0f );
	if( scale > 0.0f )
		iosLayout.scale = scale;

	IOS_EnsureTouchOverlay();
}

void IOS_Layer_GetLayout( iosLayout_t *layout )
{
	if( layout )
		*layout = iosLayout;
}

void IOS_Layer_SetGameOverlayVisible( qboolean visible )
{
	iosOverlayVisible = visible;
}

void IOS_Layer_UpdateTouchControls( qboolean visible, float opacity,
	float moveX, float moveY, float moveRadius,
	float jumpX, float jumpY, float jumpRadius, qboolean jumpActive,
	float crouchX, float crouchY, float crouchRadius, qboolean crouchActive,
	float useX, float useY, float useRadius, qboolean useActive,
	float weaponX, float weaponY, float weaponRadius,
	float menuX, float menuY, float menuRadius,
	float configX, float configY, float configRadius, qboolean configActive,
	qboolean editMode, float sliderX, float sliderY, float sliderW,
	float sliderValue )
{
	iosTouchOverlay.visible = visible;
	iosTouchOverlay.opacity = opacity;
	iosTouchOverlay.moveX = moveX;
	iosTouchOverlay.moveY = moveY;
	iosTouchOverlay.moveRadius = moveRadius;
	iosTouchOverlay.jumpX = jumpX;
	iosTouchOverlay.jumpY = jumpY;
	iosTouchOverlay.jumpRadius = jumpRadius;
	iosTouchOverlay.jumpActive = jumpActive;
	iosTouchOverlay.crouchX = crouchX;
	iosTouchOverlay.crouchY = crouchY;
	iosTouchOverlay.crouchRadius = crouchRadius;
	iosTouchOverlay.crouchActive = crouchActive;
	iosTouchOverlay.useX = useX;
	iosTouchOverlay.useY = useY;
	iosTouchOverlay.useRadius = useRadius;
	iosTouchOverlay.useActive = useActive;
	iosTouchOverlay.weaponX = weaponX;
	iosTouchOverlay.weaponY = weaponY;
	iosTouchOverlay.weaponRadius = weaponRadius;
	iosTouchOverlay.menuX = menuX;
	iosTouchOverlay.menuY = menuY;
	iosTouchOverlay.menuRadius = menuRadius;
	iosTouchOverlay2.configX = configX;
	iosTouchOverlay2.configY = configY;
	iosTouchOverlay2.configRadius = configRadius;
	iosTouchOverlay2.configActive = configActive;
	iosTouchOverlay2.editMode = editMode;
	iosTouchOverlay2.sliderX = sliderX;
	iosTouchOverlay2.sliderY = sliderY;
	iosTouchOverlay2.sliderW = sliderW;
	iosTouchOverlay2.sliderValue = sliderValue;

	IOS_Layer_Tick();
}

void IOS_Layer_OpenTouchSettings( void )
{
	IOS_Layer_Tick();
}

void IOS_Layer_AttachToWindow( void )
{
	IOS_EnsureTouchOverlay();
}
