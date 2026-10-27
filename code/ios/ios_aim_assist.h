#ifndef IOS_AIM_ASSIST_H
#define IOS_AIM_ASSIST_H

typedef enum {
	IOS_AIM_TOUCH,
	IOS_AIM_GAMEPAD,
	IOS_AIM_SOURCE_COUNT
} iosAimSource_t;

void IOS_AimAssist_Init( void );
/* Only gameplay look input from touch or the right stick belongs here. */
void IOS_AimAssist_QueueLook( float dx, float dy, iosAimSource_t source );

#endif
