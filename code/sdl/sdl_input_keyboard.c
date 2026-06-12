#include <SDL.h>

#include "../client/client.h"
#include "sdl_input_keyboard.h"

#define CTRL(a) ((a)-'a'+1)

static cvar_t *in_keyboardDebug = NULL;
static int in_keyboardEventTime = 0;
static keyNum_t in_keyboardLastKeyDown = 0;
static qboolean in_keyboardTextInputActive = qfalse;

static void IN_Keyboard_PrintKey( const SDL_Keysym *keysym, keyNum_t key, qboolean down )
{
	if( down )
		Com_Printf( "+ " );
	else
		Com_Printf( "  " );

	Com_Printf( "Scancode: 0x%02x(%s) Sym: 0x%02x(%s)",
			keysym->scancode, SDL_GetScancodeName( keysym->scancode ),
			keysym->sym, SDL_GetKeyName( keysym->sym ) );

	if( keysym->mod & KMOD_LSHIFT )   Com_Printf( " KMOD_LSHIFT" );
	if( keysym->mod & KMOD_RSHIFT )   Com_Printf( " KMOD_RSHIFT" );
	if( keysym->mod & KMOD_LCTRL )    Com_Printf( " KMOD_LCTRL" );
	if( keysym->mod & KMOD_RCTRL )    Com_Printf( " KMOD_RCTRL" );
	if( keysym->mod & KMOD_LALT )     Com_Printf( " KMOD_LALT" );
	if( keysym->mod & KMOD_RALT )     Com_Printf( " KMOD_RALT" );
	if( keysym->mod & KMOD_LGUI )     Com_Printf( " KMOD_LGUI" );
	if( keysym->mod & KMOD_RGUI )     Com_Printf( " KMOD_RGUI" );
	if( keysym->mod & KMOD_NUM )      Com_Printf( " KMOD_NUM" );
	if( keysym->mod & KMOD_CAPS )     Com_Printf( " KMOD_CAPS" );
	if( keysym->mod & KMOD_MODE )     Com_Printf( " KMOD_MODE" );

	Com_Printf( " Q:0x%02x(%s)\n", key, Key_KeynumToString( key ) );
}

#define MAX_CONSOLE_KEYS 16

static qboolean IN_Keyboard_IsConsoleKey( keyNum_t key, int character )
{
	typedef struct consoleKey_s
	{
		enum
		{
			QUAKE_KEY,
			CHARACTER
		} type;

		union
		{
			keyNum_t key;
			int character;
		} u;
	} consoleKey_t;

	static consoleKey_t consoleKeys[ MAX_CONSOLE_KEYS ];
	static int numConsoleKeys = 0;
	int i;

	if( cl_consoleKeys->modified )
	{
		char *text_p, *token;

		cl_consoleKeys->modified = qfalse;
		text_p = cl_consoleKeys->string;
		numConsoleKeys = 0;

		while( numConsoleKeys < MAX_CONSOLE_KEYS )
		{
			consoleKey_t *c = &consoleKeys[ numConsoleKeys ];
			int charCode = 0;

			token = COM_Parse( &text_p );
			if( !token[ 0 ] )
				break;

			charCode = Com_HexStrToInt( token );

			if( charCode > 0 )
			{
				c->type = CHARACTER;
				c->u.character = charCode;
			}
			else
			{
				c->type = QUAKE_KEY;
				c->u.key = Key_StringToKeynum( token );

				if( c->u.key <= 0 )
					continue;
			}

			numConsoleKeys++;
		}
	}

	if( key == character )
		key = 0;

	for( i = 0; i < numConsoleKeys; i++ )
	{
		consoleKey_t *c = &consoleKeys[ i ];

		switch( c->type )
		{
			case QUAKE_KEY:
				if( key && c->u.key == key )
					return qtrue;
				break;

			case CHARACTER:
				if( c->u.character == character )
					return qtrue;
				break;
		}
	}

	return qfalse;
}

static keyNum_t IN_Keyboard_TranslateSDLToQ3Key( SDL_Keysym *keysym, qboolean down )
{
	keyNum_t key = 0;

	if( keysym->scancode >= SDL_SCANCODE_1 && keysym->scancode <= SDL_SCANCODE_0 )
	{
		if( keysym->scancode == SDL_SCANCODE_0 )
			key = '0';
		else
			key = '1' + keysym->scancode - SDL_SCANCODE_1;
	}
	else if( keysym->sym >= SDLK_SPACE && keysym->sym < SDLK_DELETE )
	{
		key = (int)keysym->sym;
	}
	else
	{
		switch( keysym->sym )
		{
			case SDLK_PAGEUP:       key = K_PGUP;          break;
			case SDLK_KP_9:         key = K_KP_PGUP;       break;
			case SDLK_PAGEDOWN:     key = K_PGDN;          break;
			case SDLK_KP_3:         key = K_KP_PGDN;       break;
			case SDLK_KP_7:         key = K_KP_HOME;       break;
			case SDLK_HOME:         key = K_HOME;          break;
			case SDLK_KP_1:         key = K_KP_END;        break;
			case SDLK_END:          key = K_END;           break;
			case SDLK_KP_4:         key = K_KP_LEFTARROW;  break;
			case SDLK_LEFT:         key = K_LEFTARROW;     break;
			case SDLK_KP_6:         key = K_KP_RIGHTARROW; break;
			case SDLK_RIGHT:        key = K_RIGHTARROW;    break;
			case SDLK_KP_2:         key = K_KP_DOWNARROW;  break;
			case SDLK_DOWN:         key = K_DOWNARROW;     break;
			case SDLK_KP_8:         key = K_KP_UPARROW;    break;
			case SDLK_UP:           key = K_UPARROW;       break;
			case SDLK_ESCAPE:       key = K_ESCAPE;        break;
			case SDLK_KP_ENTER:     key = K_KP_ENTER;      break;
			case SDLK_RETURN:       key = K_ENTER;         break;
			case SDLK_TAB:          key = K_TAB;           break;
			case SDLK_F1:           key = K_F1;            break;
			case SDLK_F2:           key = K_F2;            break;
			case SDLK_F3:           key = K_F3;            break;
			case SDLK_F4:           key = K_F4;            break;
			case SDLK_F5:           key = K_F5;            break;
			case SDLK_F6:           key = K_F6;            break;
			case SDLK_F7:           key = K_F7;            break;
			case SDLK_F8:           key = K_F8;            break;
			case SDLK_F9:           key = K_F9;            break;
			case SDLK_F10:          key = K_F10;           break;
			case SDLK_F11:          key = K_F11;           break;
			case SDLK_F12:          key = K_F12;           break;
			case SDLK_F13:          key = K_F13;           break;
			case SDLK_F14:          key = K_F14;           break;
			case SDLK_F15:          key = K_F15;           break;
			case SDLK_BACKSPACE:    key = K_BACKSPACE;     break;
			case SDLK_KP_PERIOD:    key = K_KP_DEL;        break;
			case SDLK_DELETE:       key = K_DEL;           break;
			case SDLK_PAUSE:        key = K_PAUSE;         break;
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:       key = K_SHIFT;         break;
			case SDLK_LCTRL:
			case SDLK_RCTRL:        key = K_CTRL;          break;
#ifdef __APPLE__
			case SDLK_RGUI:
			case SDLK_LGUI:         key = K_COMMAND;       break;
#else
			case SDLK_RGUI:
			case SDLK_LGUI:         key = K_SUPER;         break;
#endif
			case SDLK_RALT:
			case SDLK_LALT:         key = K_ALT;           break;
			case SDLK_KP_5:         key = K_KP_5;          break;
			case SDLK_INSERT:       key = K_INS;           break;
			case SDLK_KP_0:         key = K_KP_INS;        break;
			case SDLK_KP_MULTIPLY:  key = K_KP_STAR;       break;
			case SDLK_KP_PLUS:      key = K_KP_PLUS;       break;
			case SDLK_KP_MINUS:     key = K_KP_MINUS;      break;
			case SDLK_KP_DIVIDE:    key = K_KP_SLASH;       break;
			case SDLK_MODE:         key = K_MODE;          break;
			case SDLK_HELP:         key = K_HELP;          break;
			case SDLK_PRINTSCREEN:  key = K_PRINT;         break;
			case SDLK_SYSREQ:       key = K_SYSREQ;        break;
			case SDLK_MENU:         key = K_MENU;          break;
			case SDLK_APPLICATION:	key = K_MENU;          break;
			case SDLK_POWER:        key = K_POWER;         break;
			case SDLK_UNDO:         key = K_UNDO;          break;
			case SDLK_SCROLLLOCK:   key = K_SCROLLOCK;     break;
			case SDLK_NUMLOCKCLEAR: key = K_KP_NUMLOCK;    break;
			case SDLK_CAPSLOCK:     key = K_CAPSLOCK;      break;
			default:
				if( !( keysym->sym & SDLK_SCANCODE_MASK ) && keysym->scancode <= 95 )
					key = K_WORLD_0 + (int)keysym->scancode;
				break;
		}
	}

	if( in_keyboardDebug && in_keyboardDebug->integer )
		IN_Keyboard_PrintKey( keysym, key, down );

	if( IN_Keyboard_IsConsoleKey( key, 0 ) )
		key = K_CONSOLE;

	return key;
}

static void IN_Keyboard_ProcessTextInput( const char *text )
{
	char *c = (char *)text;

	if( in_keyboardLastKeyDown == K_CONSOLE )
		return;

	while( *c )
	{
		int utf32 = 0;

		if( ( *c & 0x80 ) == 0 )
			utf32 = *c++;
		else if( ( *c & 0xE0 ) == 0xC0 )
		{
			utf32 |= ( *c++ & 0x1F ) << 6;
			utf32 |= ( *c++ & 0x3F );
		}
		else if( ( *c & 0xF0 ) == 0xE0 )
		{
			utf32 |= ( *c++ & 0x0F ) << 12;
			utf32 |= ( *c++ & 0x3F ) << 6;
			utf32 |= ( *c++ & 0x3F );
		}
		else if( ( *c & 0xF8 ) == 0xF0 )
		{
			utf32 |= ( *c++ & 0x07 ) << 18;
			utf32 |= ( *c++ & 0x3F ) << 12;
			utf32 |= ( *c++ & 0x3F ) << 6;
			utf32 |= ( *c++ & 0x3F );
		}
		else
		{
			Com_DPrintf( "Unrecognised UTF-8 lead byte: 0x%x\n", (unsigned int)*c );
			c++;
		}

		if( utf32 != 0 )
		{
			if( IN_Keyboard_IsConsoleKey( 0, utf32 ) )
			{
				Com_QueueEvent( in_keyboardEventTime, SE_KEY, K_CONSOLE, qtrue, 0, NULL );
				Com_QueueEvent( in_keyboardEventTime, SE_KEY, K_CONSOLE, qfalse, 0, NULL );
			}
			else
				Com_QueueEvent( in_keyboardEventTime, SE_CHAR, utf32, 0, 0, NULL );
		}
	}
}

void IN_Keyboard_Init( void )
{
	in_keyboardDebug = Cvar_Get( "in_keyboardDebug", "0", CVAR_ARCHIVE );
	SDL_SetHint( SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0" );
	in_keyboardLastKeyDown = 0;
	in_keyboardTextInputActive = qfalse;
}

void IN_Keyboard_Shutdown( void )
{
	if( in_keyboardTextInputActive )
	{
		SDL_StopTextInput();
		in_keyboardTextInputActive = qfalse;
	}
	in_keyboardLastKeyDown = 0;
}

void IN_Keyboard_SetEventTime( int eventTime )
{
	in_keyboardEventTime = eventTime;
}

qboolean IN_Keyboard_ProcessEvent( SDL_Event *event )
{
	keyNum_t key;

	switch( event->type )
	{
		case SDL_KEYDOWN:
			if( event->key.repeat && Key_GetCatcher() == 0 )
				return qtrue;

			key = IN_Keyboard_TranslateSDLToQ3Key( &event->key.keysym, qtrue );
			if( key )
			{
				Com_QueueEvent( in_keyboardEventTime, SE_KEY, key, qtrue, 0, NULL );

				if( key == K_BACKSPACE )
					Com_QueueEvent( in_keyboardEventTime, SE_CHAR, CTRL('h'), 0, 0, NULL );
				else if( keys[K_CTRL].down && key >= 'a' && key <= 'z' )
					Com_QueueEvent( in_keyboardEventTime, SE_CHAR, CTRL(key), 0, 0, NULL );
				else if( key >= K_SPACE && key < K_BACKSPACE )
					Com_QueueEvent( in_keyboardEventTime, SE_CHAR, key, 0, 0, NULL );
			}

			in_keyboardLastKeyDown = key;
			return qtrue;

		case SDL_KEYUP:
			key = IN_Keyboard_TranslateSDLToQ3Key( &event->key.keysym, qfalse );
			if( key )
				Com_QueueEvent( in_keyboardEventTime, SE_KEY, key, qfalse, 0, NULL );

			in_keyboardLastKeyDown = 0;
			return qtrue;

		case SDL_TEXTINPUT:
			IN_Keyboard_ProcessTextInput( event->text.text );
			return qtrue;

		default:
			return qfalse;
	}
}

void IN_Keyboard_UpdateTextInput( void )
{
	qboolean wantText = ( Key_GetCatcher() & ( KEYCATCH_UI | KEYCATCH_CONSOLE | KEYCATCH_MESSAGE ) ) != 0;

	if( wantText && !in_keyboardTextInputActive )
	{
		SDL_StartTextInput();
		in_keyboardTextInputActive = qtrue;
	}
	else if( !wantText && in_keyboardTextInputActive )
	{
		SDL_StopTextInput();
		in_keyboardTextInputActive = qfalse;
	}
}
