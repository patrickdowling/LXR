#include "menu.h"
#include "..\Hardware\lcd.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "..\ledHandler.h"
#include <avr/pgmspace.h>
#include "..\IO/uart.h"


#include "..\Preset/PresetManager.h"
#include "..\frontPanelParser.h"
//#include <util/delay.h>
#include <util/delay.h>

#include "menuPages.h"
#include "menuText.h"
#include "../Hardware/timebase.h"
#include "screensaver.h"
#include "CcNr2Text.h"
#include "copyClearTools.h"
#include "../buttonHandler.h"
#include <ctype.h>
#include "../front.h"

/* fill up to 3 bytes of a buffer with string representation of a number */
// space padded unsigned
static void numtostrpu(char *buf, uint8_t num);
// space padded signed
static void numtostrps(char *buf, int8_t num);
// non space padded unsigned
static void numtostru(char *buf, uint8_t num);
// non space padded signed
static void numtostrs(char *buf, int8_t num);
// uppercase 3 letters in buf
static void upr_three(char *buf);



#define ARROW_SIGN '>'
//-----------------------------------------------------------------
//vars
//-----------------------------------------------------------------
/** the lower 3 bit indicate the active parameter.
the upper bits indicate the active page no.
 */
static uint8_t menuIndex = 0; 

//preset vars
#define NUM_PRESET_LOCATIONS 3 //kit, pattern, morph sound
static uint8_t menu_currentPresetNr[NUM_PRESET_LOCATIONS];

uint8_t menu_shownPattern = 0;
uint8_t menu_muteModeActive = 0;

static void setNoteName(uint8_t num, char *buf);

void menu_setShownPattern(uint8_t patternNr)
{
	menu_shownPattern = patternNr;
	frontPanel_sendData(SEQ_CC,SEQ_SET_SHOWN_PATTERN,menu_shownPattern);
}

uint8_t menu_getViewedPattern()
{
	return menu_shownPattern;
}


//enum for the save what parameter
enum loadSaveEnum
{
	WHAT_KIT = 0,
	WHAT_PATTERN,
	WHAT_MORPH,
	WHAT_GLO,
};

enum saveStateEnum
{
	SAVE_STATE_EDIT_TYPE,
	SAVE_STATE_EDIT_PRESET_NR,
	SAVE_STATE_EDIT_NAME1,
	SAVE_STATE_EDIT_NAME2,
	SAVE_STATE_EDIT_NAME3,
	SAVE_STATE_EDIT_NAME4,
	SAVE_STATE_EDIT_NAME5,
	SAVE_STATE_EDIT_NAME6,
	SAVE_STATE_EDIT_NAME7,
	SAVE_STATE_EDIT_NAME8,
	SAVE_STATE_OK,
};

/** a struct for some save page parameters.*/
static volatile struct {
	unsigned what:3;		/**< 0= save kit, 1 = save pattern, 2 = morph sound */
	unsigned state:4;		/**< 0=edit kit/pat, 1=edit preset nr, 2-9 = edit name, 10=ok*/

} menu_saveOptions;
//static volatile char menu_currentPresetName[9]; //8 chars + '\0'

/** buffer to minimize the display configuration.
It holds a representation of the display content so only the changed cells have to be updated*/
char currentDisplayBuffer[2][16];	/**< what is currently shown on the LCD*/
char editDisplayBuffer[2][17];		/**< what should be shown on the LCD after the next update. length 17 to allow zero termination without overflow.*/
uint8_t menu_activePage = 0;				/**< indicates which menu page is currently shown*/
uint8_t menu_activeVoice = 0;
uint8_t menu_playedPattern = 0;
static uint8_t editModeActive = 0;			/**< when edit mode is active, only the currently active parameter is shown with full name*/
static uint8_t lastEncoderButton = 0;		/**< stores the state of the encoder button so the edit mode is only switched once when the button is pressed*/

static uint8_t parameterFetch = 0b00011111;	/**< the lower 4 bits define a lock for each pot, the 5 bit turns he lock on and off*/

//Macro to check the param lock bit in the parameterFetch var
#define PARAMETER_LOCK_ACTIVE (1<<4)


//-----------------------------------------------------------------
/** array holding all the names for the parameters
 *  short name, category, long name
 * These correspond with NamesEnum
 * */
const Name valueNames[NUM_NAMES] PROGMEM =
{
		{SHORT_EMPTY,CAT_EMPTY,LONG_EMPTY},					//empty
		{SHORT_COARSE,CAT_OSC,LONG_COARSE},					//coarse tune
		{SHORT_FINE,CAT_OSC,LONG_FINE},						//fine tune
		{SHORT_ATTACK,CAT_VELO_EG,LONG_ATTACK},				//attack velocity
		{SHORT_DECAY,CAT_VELO_EG,LONG_DECAY},				//decay velocity
		{SHORT_DECAY,CAT_PITCH_EG,LONG_DECAY},				//pitch decay
		{SHORT_MOD,CAT_PITCH_MOD,LONG_AMOUNT},				//mod amount
		{SHORT_FM_AMNT,CAT_FM,LONG_AMOUNT},					//fm amount
		{SHORT_FREQ,CAT_FM,LONG_FREQ},						//fm frequency
		{SHORT_DRIVE, CAT_VOICE,LONG_DRIVE},				//drive
		{SHORT_VOL,CAT_VOICE,LONG_VOL},						//volume
		{SHORT_PAN,CAT_VOICE,LONG_PAN},						//pan
		{SHORT_NOISE,CAT_NOISE,LONG_FREQ},					//noise frequency
		{SHORT_MIX,CAT_NOISE_OSC,LONG_MIX},					//mix
		{SHORT_REPEAT,CAT_VELO_EG,LONG_REPEAT_CNT},			//repeat
		{SHORT_FREQ,CAT_FILTER,LONG_FREQ},					//filter freq
		{SHORT_FIL_RESO,CAT_FILTER,LONG_RESONANCE},			//filter reso
		{SHORT_FIL_TYPE,CAT_FILTER,LONG_TYPE},				//filter type
		{SHORT_MOD_OSC1_FREQ,CAT_MOD_OSC,LONG_FREQ},		//mod osc 1 freq
		{SHORT_MOD_OSC2_FREQ,CAT_MOD_OSC,LONG_FREQ},		//mod osc 2 freq
		{SHORT_MOD_OSC1_GAIN,CAT_MOD_OSC,LONG_GAIN},		//mod osc 1 gain
		{SHORT_MOD_OSC2_GAIN,CAT_MOD_OSC,LONG_GAIN},		//mod osc 2 gain

		{SHORT_FREQ,CAT_LFO,LONG_FREQ},						//freq lfo
		{SHORT_MOD,CAT_LFO,LONG_AMOUNT},					//mod lfo
		{SHORT_WAVE,CAT_LFO,LONG_WAVE},						//wave lfo
		{SHORT_DEST,CAT_LFO,LONG_DEST_PARAM},				//destination lfo (destination)

		{SHORT_SYNC,CAT_LFO,LONG_CLOCKSYNC},				//TEXT_SYNC_LFO,
		{SHORT_RETRIGGER,CAT_LFO,LONG_RETRIGGER},			//TEXT_RETRIGGER_LFO,
		{SHORT_OFFSET,CAT_LFO,LONG_OFFSET},					//TEXT_OFFSET_LFO,
		{SHORT_VOICE,CAT_LFO,LONG_DEST_VOICE},				//TEXT_TARGET_VOICE_LFO,

		{SHORT_SLOPE,CAT_VELO_EG,LONG_SLOPE},				//TEXT_EG_SLOPE,

		{SHORT_DECAY1,CAT_VELO_EG,LONG_DECAY_CLOSED},		//TEXT_DECAY_CLOSED,
		{SHORT_DECAY2,CAT_VELO_EG,LONG_DECAY_OPEN},			//TEXT_DECAY_OPEN,

		{SHORT_WAVE,CAT_OSC,LONG_WAVE},						//TEXT_WAVEFORM

		{SHORT_WAVE,CAT_TRANS,LONG_WAVE},					//TEXT_TRANSIENT_WAVE
		{SHORT_VOL,CAT_TRANS,LONG_VOL},						//TEXT_TRANSIENT_VOLUME
		{SHORT_FREQ,CAT_TRANS,LONG_FREQ},					//TEXT_TRANSIENT_FREQ


		{SHORT_EQ_GAIN,CAT_EQ,LONG_GAIN},					//TEXT_EQ_GAIN
		{SHORT_EQ_FREQ,CAT_EQ,LONG_FREQ},					//TEXT_EQ_FREQ

#if USE_CODEC_EQ

		{SHORT_EQ,CAT_EQ,LONG_ENABLE},						//TEXT_CODEC_EQ_ENABLE
		{SHORT_MOD_OSC1_FREQ,CAT_EQ,LONG_FREQ},				//TEXT_CODEC_EQ_FB
		{SHORT_MOD_OSC2_FREQ,CAT_EQ,LONG_FREQ},				//TEXT_CODEC_EQ_FT
		{SHORT_MOD_OSC1_GAIN,CAT_EQ,LONG_GAIN},				//TEXT_CODEC_EQ_GB
		{SHORT_MOD_OSC2_GAIN,CAT_EQ,LONG_GAIN},				//TEXT_CODEC_EQ_GT

		{SHORT_LIMITER,CAT_LIMIT,LONG_ENABLE},				//TEXT_CODEC_LIMIT_ENABLE
		{SHORT_ATTACK,CAT_LIMIT,LONG_ATTACK},				//TEXT_CODEC_LIMIT_ATTACK
		{SHORT_RELEASE,CAT_LIMIT,LONG_RELEASE},				//TEXT_CODEC_LIMIT_RELEASE
		{SHORT_MAX,CAT_LIMIT,LONG_THRESH_MAX},				//TEXT_CODEC_LIMIT_MAX
		{SHORT_MIN,CAT_LIMIT,LONG_THRESH_MIN},				//TEXT_CODEC_LIMIT_MIN

#endif

		{SHORT_ROLL,CAT_PATTERN,LONG_ROLLRATE},				//TEXT_ROLL_SPEED
		{SHORT_MORPH,CAT_SOUND,LONG_MORPH},					//TEXT_X_FADE


		{SHORT_VELOCITY,CAT_STEP,LONG_VELOCITY},			//TEXT_STEP_VELOCITY
		{SHORT_NOTE,CAT_STEP,LONG_NOTE},					//TEXT_NOTE
		{SHORT_PROBABILITY,CAT_STEP,LONG_PROBABILITY},		//TEXT_PROBABILITY,
		{SHORT_STEP,CAT_STEP,LONG_NUMBER},					//TEXT_ACTIVE_STEP

		{SHORT_LENGTH,CAT_PATTERN,LONG_LENGTH},				//TEXT_PAT_LENGTH,
		{SHORT_STEP,CAT_EUKLID,LONG_STEPS},					//TEXT_NUM_STEPS,

		{SHORT_BPM,CAT_GLOBAL,LONG_TEMPO},					//TEXT_BPM

		{SHORT_CHANNEL,CAT_VOICE,LONG_MIDI_CHANNEL},		//TEXT_MIDI_CHANNEL
		{SHORT_OUT,CAT_VOICE,LONG_AUDIO_OUT},				//TEXT_AUDIO_OUT

		{SHORT_SR,CAT_VOICE,LONG_SAMPLE_RATE},				//TEXT_SAMPLE_RATE

		{SHORT_REPEAT,CAT_PATTERN,LONG_REPEAT_CNT},				//TEXT_PATTERN_REPEAT
		{SHORT_NXT,CAT_PATTERN,LONG_NEXT_PAT},				//TEXT_PATTERN_NEXT

		//	{SHORT_PHASE,CAT_OSC,LONG_PHASE},				//TEXT_OSC_PHASE
		{SHORT_MODE,CAT_OSC,LONG_MODE},				//TEXT_MODE mix or mod for fm osc
		{SHORT_VOL,CAT_OSC,LONG_VOL},				//TEXT_OSC_VOLUME fm oscillator volume in mix mode
		{SHORT_DRIVE, CAT_FILTER,LONG_DRIVE},			//TEXT_FILTER_DRIVE

		{SHORT_DEST,	CAT_VELOCITY,	LONG_DEST_PARAM},		//TEXT_VEL_DEST,
		{SHORT_FM_AMNT,	CAT_VELOCITY,	LONG_AMOUNT},			//TEXT_VEL_AMT,
		{SHORT_VOL,CAT_VELOCITY,	LONG_VOLUME_MOD},		//TEXT_VEL_MOD_VOL,

		{SHORT_FETCH,	CAT_PARAMETER,	LONG_FETCH},		//TEXT_FETCH
		{SHORT_FOLLOW,	CAT_SEQUENCER,	LONG_FOLLOW},		//TEXT_FOLLOW
		{SHORT_QUANT,	CAT_SEQUENCER,	LONG_QUANTISATION},			//TEXT_QUANTISATION

		{SHORT_TRACK,	CAT_SEQUENCER,	LONG_AUTOMATION_TRACK},			//TEXT_AUTOMATION_TRACK

		{SHORT_DEST,	CAT_SEQUENCER,	LONG_AUTOMATION_DEST},	//TEXT_PARAM_DEST
		{SHORT_VALUE,	CAT_SEQUENCER,	LONG_AUTOMATION_VAL},	//TEXT_PARAM_VAL

		{SHORT_SHUFFLE,	CAT_SEQUENCER,	LONG_SHUFFLE},	//TEXT_SHUFFLE

		{SHORT_SCREEN_SAVER, CAT_GLOBAL,LONG_SCREENSAVER},	//TEXT_SCREENSAVER_ON_OFF

		{SHORT_EMPTY,CAT_EMPTY,LONG_EMPTY},					//SKIP

		{SHORT_X,CAT_GENERATOR,LONG_X},	//TEXT_POS_X,
		{SHORT_Y,CAT_GENERATOR,LONG_Y},	//TEXT_POS_Y,
		{SHORT_FLUX,CAT_GENERATOR,LONG_FLUX},	//TEXT_FLUX,
		{SHORT_FREQ,CAT_GENERATOR,LONG_FREQ},	//TEXT_SOM_FREQ,
		{SHORT_MIDI,CAT_MIDI,LONG_MODE},	//TEXT_MIDI_MODE
};



//---------------------------------------------------------------
// Parameter types. These correspond with ParamEnums and entries in parameter_values
const enum Datatypes PROGMEM parameter_dtypes[NUM_PARAMS] = {
	    /*PAR_NONE*/ 			DTYPE_0B127,
	    /*PAR_OSC_WAVE_DRUM1*/ 	DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_OSC_WAVE_DRUM2*/ 	DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_OSC_WAVE_DRUM3*/ 	DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_OSC_WAVE_SNARE*/  DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*NRPN_DATA_ENTRY_COARSE*/ DTYPE_0B127,
	    /*PAR_WAVE1_CYM*/  		DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_WAVE1_HH*/  		DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_COARSE1*/ 		DTYPE_0B127,
	    /*PAR_FINE1*/ 			DTYPE_PM63,
	    /*PAR_COARSE2*/			DTYPE_0B127,
	    /*PAR_FINE2*/ 			DTYPE_PM63,
	    /*PAR_COARSE3*/ 		DTYPE_0B127,
	    /*PAR_FINE3*/ 			DTYPE_PM63,
	    /*PAR_COARSE4*/ 		DTYPE_0B127,
	    /*PAR_FINE4*/ 			DTYPE_PM63,
	    /*PAR_COARSE5*/			DTYPE_0B127,
	    /*PAR_FINE5*/ 			DTYPE_PM63,
	    /*PAR_COARSE6*/ 		DTYPE_0B127,
	    /*PAR_FINE6*/ 			DTYPE_PM63,
	    /*PAR_MOD_WAVE_DRUM1*/  DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_MOD_WAVE_DRUM2*/  DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_MOD_WAVE_DRUM3*/  DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_WAVE2_CYM*/ 		DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_WAVE3_CYM*/ 		DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_WAVE2_HH*/ 		DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_WAVE3_HH*/ 		DTYPE_MENU | (MENU_WAVEFORM<<4),
	    /*PAR_NOISE_FREQ1*/ 	DTYPE_0B127,
	    /*PAR_MIX1*/ 			DTYPE_0B127,
	    /*PAR_MOD_OSC_F1_CYM*/ 	DTYPE_0B127,
	    /*PAR_MOD_OSC_F2_CYM*/ 	DTYPE_0B127,
	    /*PAR_MOD_OSC_GAIN1_CYM*/ DTYPE_0B127,
	    /*PAR_MOD_OSC_GAIN2_CYM*/ DTYPE_0B127,
	    /*PAR_MOD_OSC_F1*/ 		DTYPE_0B127,
	    /*PAR_MOD_OSC_F2*/ 		DTYPE_0B127,
	    /*PAR_MOD_OSC_GAIN1*/ 	DTYPE_0B127,
	    /*PAR_MOD_OSC_GAIN2*/ 	DTYPE_0B127,
	    /*PAR_FILTER_FREQ_1*/ 	DTYPE_0B127,
	    /*PAR_FILTER_FREQ_2*/ 	DTYPE_0B127,
	    /*PAR_FILTER_FREQ_3*/ 	DTYPE_0B127,
	    /*PAR_FILTER_FREQ_4*/ 	DTYPE_0B127,
	    /*PAR_FILTER_FREQ_5*/ 	DTYPE_0B127,
	    /*PAR_FILTER_FREQ_6*/ 	DTYPE_0B127,
	    /*PAR_RESO_1*/ 			DTYPE_0B127,
	    /*PAR_RESO_2*/ 			DTYPE_0B127,
	    /*PAR_RESO_3*/ 			DTYPE_0B127,
	    /*PAR_RESO_4*/ 			DTYPE_0B127,
	    /*PAR_RESO_5*/ 			DTYPE_0B127,
	    /*PAR_RESO_6*/ 			DTYPE_0B127,
	    /*PAR_VELOA1*/ 			DTYPE_0B127,
	    /*PAR_VELOD1*/ 			DTYPE_0B127,
	    /*PAR_VELOA2*/ 			DTYPE_0B127,
	    /*PAR_VELOD2*/ 			DTYPE_0B127,
	    /*PAR_VELOA3*/ 			DTYPE_0B127,
	    /*PAR_VELOD3*/ 			DTYPE_0B127,
	    /*PAR_VELOA4*/ 			DTYPE_0B127,
	    /*PAR_VELOD4*/ 			DTYPE_0B127,
	    /*PAR_VELOA5*/ 			DTYPE_0B127,
	    /*PAR_VELOD5*/ 			DTYPE_0B127,
	    /*PAR_VELOA6*/ 			DTYPE_0B127,
	    /*PAR_VELOD6_CLOSED*/ 	DTYPE_0B127,
	    /*PAR_VELOD6_OPEN*/ 	DTYPE_0B127,
	    /*PAR_VOL_SLOPE1*/ 		DTYPE_0B127,
	    /*PAR_VOL_SLOPE2*/ 		DTYPE_0B127,
	    /*PAR_VOL_SLOPE3*/ 		DTYPE_0B127,
	    /*PAR_VOL_SLOPE4*/ 		DTYPE_0B127,
	    /*PAR_VOL_SLOPE5*/ 		DTYPE_0B127,
	    /*PAR_VOL_SLOPE6*/ 		DTYPE_0B127,
	    /*PAR_REPEAT4*/ 		DTYPE_0B127,
	    /*PAR_REPEAT5*/ 		DTYPE_0B127,
	    /*PAR_MOD_EG1*/ 		DTYPE_0B127,
	    /*PAR_MOD_EG2*/ 		DTYPE_0B127,
	    /*PAR_MOD_EG3*/ 		DTYPE_0B127,
	    /*PAR_MOD_EG4*/ 		DTYPE_0B127,
	    /*PAR_MODAMNT1*/ 		DTYPE_0B127,
	    /*PAR_MODAMNT2*/ 		DTYPE_0B127,
	    /*PAR_MODAMNT3*/ 		DTYPE_0B127,
	    /*PAR_MODAMNT4*/ 		DTYPE_0B127,
	    /*PAR_PITCH_SLOPE1*/ 	DTYPE_0B127,
	    /*PAR_PITCH_SLOPE2*/ 	DTYPE_0B127,
	    /*PAR_PITCH_SLOPE3*/ 	DTYPE_0B127,
	    /*PAR_PITCH_SLOPE4*/ 	DTYPE_0B127,
	    /*PAR_FMAMNT1*/ 		DTYPE_0B127,
	    /*PAR_FM_FREQ1*/ 		DTYPE_0B127,
	    /*PAR_FMAMNT2*/ 		DTYPE_0B127,
	    /*PAR_FM_FREQ2*/ 		DTYPE_0B127,
	    /*PAR_FMAMNT3*/ 		DTYPE_0B127,
	    /*PAR_FM_FREQ3*/ 		DTYPE_0B127,
	    /*PAR_VOL1*/ 			DTYPE_0B127,
	    /*PAR_VOL2*/ 			DTYPE_0B127,
	    /*PAR_VOL3*/ 			DTYPE_0B127,
	    /*PAR_VOL4*/ 			DTYPE_0B127,
	    /*PAR_VOL5*/ 			DTYPE_0B127,
	    /*PAR_VOL6*/ 			DTYPE_0B127,
	    /*PAR_PAN1*/ 			DTYPE_PM63,
	    /*PAR_PAN2*/ 			DTYPE_PM63,
	    /*PAR_PAN3*/ 			DTYPE_PM63,
	    /*NRPN_FINE*/ 			DTYPE_0B127,
	    /*NRPN_COARSE*/ 		DTYPE_0B127,
	    /*PAR_PAN4*/ 			DTYPE_PM63,
	    /*PAR_PAN5*/ 			DTYPE_PM63,
	    /*PAR_PAN6*/ 			DTYPE_PM63,
	    /*PAR_DRIVE1*/ 			DTYPE_0B127,
	    /*PAR_DRIVE2*/ 			DTYPE_0B127,
	    /*PAR_DRIVE3*/ 			DTYPE_0B127,
	    /*PAR_SNARE_DISTORTION*/ DTYPE_0B127,
	    /*PAR_CYMBAL_DISTORTION*/ DTYPE_0B127,
	    /*PAR_HAT_DISTORTION*/ 	DTYPE_0B127,
	    /*PAR_VOICE_DECIMATION1*/ DTYPE_0B127,
	    /*PAR_VOICE_DECIMATION2*/ DTYPE_0B127,
	    /*PAR_VOICE_DECIMATION3*/ DTYPE_0B127,
	    /*PAR_VOICE_DECIMATION4*/ DTYPE_0B127,
	    /*PAR_VOICE_DECIMATION5*/ DTYPE_0B127,
	    /*PAR_VOICE_DECIMATION6*/ DTYPE_0B127,
	    /*PAR_VOICE_DECIMATION_ALL */ DTYPE_0B127,
	    /*PAR_FREQ_LFO1 */ 		DTYPE_0B127,
	    /*PAR_FREQ_LFO2*/		DTYPE_0B127,
	    /*PAR_FREQ_LFO3*/ 		DTYPE_0B127,
	    /*PAR_FREQ_LFO4*/ 		DTYPE_0B127,
	    /*PAR_FREQ_LFO5*/ 		DTYPE_0B127,
	    /*PAR_FREQ_LFO6*/ 		DTYPE_0B127,
	    /*PAR_AMOUNT_LFO1*/		DTYPE_0B127,
	    /*PAR_AMOUNT_LFO2*/ 	DTYPE_0B127,
	    /*PAR_AMOUNT_LFO3*/ 	DTYPE_0B127,
	    /*PAR_AMOUNT_LFO4*/		DTYPE_0B127,
	    /*PAR_AMOUNT_LFO5*/ 	DTYPE_0B127,
	    /*PAR_AMOUNT_LFO6*/ 	DTYPE_0B127,
	    /*PAR_RESERVED4*/ 		DTYPE_0B127,
	    /*PAR_FILTER_DRIVE_1*/ 	DTYPE_0B127,
	    /*PAR_FILTER_DRIVE_2*/ 	DTYPE_0B127,
	    /*PAR_FILTER_DRIVE_3*/ 	DTYPE_0B127,
	    /*PAR_FILTER_DRIVE_4*/ 	DTYPE_0B127,
	    /*PAR_FILTER_DRIVE_5*/ 	DTYPE_0B127,
	    /*PAR_FILTER_DRIVE_6*/	DTYPE_0B127,
	    /*PAR_MIX_MOD_1*/ 		DTYPE_MIX_FM,
	    /*PAR_MIX_MOD_2*/ 		DTYPE_MIX_FM,
	    /*PAR_MIX_MOD_3*/ 		DTYPE_MIX_FM,
	    /*PAR_VOLUME_MOD_ON_OFF1*/ DTYPE_ON_OFF,
	    /*PAR_VOLUME_MOD_ON_OFF2*/ DTYPE_ON_OFF,
	    /*PAR_VOLUME_MOD_ON_OFF3*/ DTYPE_ON_OFF,
	    /*PAR_VOLUME_MOD_ON_OFF4*/ DTYPE_ON_OFF,
	    /*PAR_VOLUME_MOD_ON_OFF5*/ DTYPE_ON_OFF,
	    /*PAR_VOLUME_MOD_ON_OFF6*/ DTYPE_ON_OFF,
	    /*PAR_VELO_MOD_AMT_1*/ 	DTYPE_0B127,
	    /*PAR_VELO_MOD_AMT_2*/ 	DTYPE_0B127,
	    /*PAR_VELO_MOD_AMT_3*/ 	DTYPE_0B127,
	    /*PAR_VELO_MOD_AMT_4*/ 	DTYPE_0B127,
	    /*PAR_VELO_MOD_AMT_5*/ 	DTYPE_0B127,
	    /*PAR_VELO_MOD_AMT_6*/ 	DTYPE_0B127,
	    /*PAR_VEL_DEST_1*/ 		DTYPE_TARGET_SELECTION_VELO,
	    /*PAR_VEL_DEST_2*/ 		DTYPE_TARGET_SELECTION_VELO,
	    /*PAR_VEL_DEST_3*/ 		DTYPE_TARGET_SELECTION_VELO,
	    /*PAR_VEL_DEST_4*/ 		DTYPE_TARGET_SELECTION_VELO,
	    /*PAR_VEL_DEST_5*/ 		DTYPE_TARGET_SELECTION_VELO,
	    /*PAR_VEL_DEST_6*/ 		DTYPE_TARGET_SELECTION_VELO,
	    /*PAR_WAVE_LFO1*/ 		DTYPE_MENU | (MENU_LFO_WAVES<<4),
	    /*PAR_WAVE_LFO2*/ 		DTYPE_MENU | (MENU_LFO_WAVES<<4),
	    /*PAR_WAVE_LFO3*/ 		DTYPE_MENU | (MENU_LFO_WAVES<<4),
	    /*PAR_WAVE_LFO4 */ 		DTYPE_MENU | (MENU_LFO_WAVES<<4),
	    /*PAR_WAVE_LFO5*/ 		DTYPE_MENU | (MENU_LFO_WAVES<<4),
	    /*PAR_WAVE_LFO6*/ 		DTYPE_MENU | (MENU_LFO_WAVES<<4),
	    /*PAR_VOICE_LFO1*/ 		DTYPE_VOICE_LFO,
	    /*PAR_VOICE_LFO2*/ 		DTYPE_VOICE_LFO,
	    /*PAR_VOICE_LFO3*/ 		DTYPE_VOICE_LFO,
	    /*PAR_VOICE_LFO4*/ 		DTYPE_VOICE_LFO,
	    /*PAR_VOICE_LFO5*/ 		DTYPE_VOICE_LFO,
	    /*PAR_VOICE_LFO6*/ 		DTYPE_VOICE_LFO,
	    /*PAR_TARGET_LFO1*/ 	DTYPE_TARGET_SELECTION_LFO,
	    /*PAR_TARGET_LFO2*/ 	DTYPE_TARGET_SELECTION_LFO,
	    /*PAR_TARGET_LFO3*/ 	DTYPE_TARGET_SELECTION_LFO,
	    /*PAR_TARGET_LFO4*/ 	DTYPE_TARGET_SELECTION_LFO,
	    /*PAR_TARGET_LFO5*/ 	DTYPE_TARGET_SELECTION_LFO,
	    /*PAR_TARGET_LFO6*/ 	DTYPE_TARGET_SELECTION_LFO,
	    /*PAR_RETRIGGER_LFO1*/ 	DTYPE_MENU | (MENU_RETRIGGER<<4),
	    /*PAR_RETRIGGER_LFO2*/ 	DTYPE_MENU | (MENU_RETRIGGER<<4),
	    /*PAR_RETRIGGER_LFO3*/ 	DTYPE_MENU | (MENU_RETRIGGER<<4),
	    /*PAR_RETRIGGER_LFO4*/ 	DTYPE_MENU | (MENU_RETRIGGER<<4),
	    /*PAR_RETRIGGER_LFO5*/ 	DTYPE_MENU | (MENU_RETRIGGER<<4),
	    /*PAR_RETRIGGER_LFO6*/ 	DTYPE_MENU | (MENU_RETRIGGER<<4),
	    /*PAR_SYNC_LFO1*/ 		DTYPE_MENU | (MENU_SYNC_RATES<<4),
	    /*PAR_SYNC_LFO2*/ 		DTYPE_MENU | (MENU_SYNC_RATES<<4),
	    /*PAR_SYNC_LFO3*/ 		DTYPE_MENU | (MENU_SYNC_RATES<<4),
	    /*PAR_SYNC_LFO4*/ 		DTYPE_MENU | (MENU_SYNC_RATES<<4),
	    /*PAR_SYNC_LFO5*/ 		DTYPE_MENU | (MENU_SYNC_RATES<<4),
	    /*PAR_SYNC_LFO6*/ 		DTYPE_MENU | (MENU_SYNC_RATES<<4),
	    /*PAR_OFFSET_LFO1*/ 	DTYPE_0B127,
	    /*PAR_OFFSET_LFO2*/ 	DTYPE_0B127,
	    /*PAR_OFFSET_LFO3*/ 	DTYPE_0B127,
	    /*PAR_OFFSET_LFO4*/ 	DTYPE_0B127,
	    /*PAR_OFFSET_LFO5*/ 	DTYPE_0B127,
	    /*PAR_OFFSET_LFO6*/ 	DTYPE_0B127,
	    /*PAR_FILTER_TYPE_1*/ 	DTYPE_MENU | (MENU_FILTER<<4),
	    /*PAR_FILTER_TYPE_2*/ 	DTYPE_MENU | (MENU_FILTER<<4),
	    /*PAR_FILTER_TYPE_3*/ 	DTYPE_MENU | (MENU_FILTER<<4),
	    /*PAR_FILTER_TYPE_4*/ 	DTYPE_MENU | (MENU_FILTER<<4),
	    /*PAR_FILTER_TYPE_5*/ 	DTYPE_MENU | (MENU_FILTER<<4),
	    /*PAR_FILTER_TYPE_6*/ 	DTYPE_MENU | (MENU_FILTER<<4),
	    /*PAR_TRANS1_VOL*/ 		DTYPE_0B127,
	    /*PAR_TRANS2_VOL*/ 		DTYPE_0B127,
	    /*PAR_TRANS3_VOL*/ 		DTYPE_0B127,
	    /*PAR_TRANS4_VOL*/ 		DTYPE_0B127,
	    /*PAR_TRANS5_VOL*/ 		DTYPE_0B127,
	    /*PAR_TRANS6_VOL*/ 		DTYPE_0B127,
	    /*PAR_TRANS1_WAVE*/ 	DTYPE_MENU | (MENU_TRANS<<4),
	    /*PAR_TRANS2_WAVE*/ 	DTYPE_MENU | (MENU_TRANS<<4),
	    /*PAR_TRANS3_WAVE*/ 	DTYPE_MENU | (MENU_TRANS<<4),
	    /*PAR_TRANS4_WAVE*/ 	DTYPE_MENU | (MENU_TRANS<<4),
	    /*PAR_TRANS5_WAVE*/ 	DTYPE_MENU | (MENU_TRANS<<4),
	    /*PAR_TRANS6_WAVE*/ 	DTYPE_MENU | (MENU_TRANS<<4),
	    /*PAR_TRANS1_FREQ*/ 	DTYPE_0B127,
	    /*PAR_TRANS2_FREQ*/ 	DTYPE_0B127,
	    /*PAR_TRANS3_FREQ*/ 	DTYPE_0B127,
	    /*PAR_TRANS4_FREQ*/ 	DTYPE_0B127,
	    /*PAR_TRANS5_FREQ*/ 	DTYPE_0B127,
	    /*PAR_TRANS6_FREQ*/ 	DTYPE_0B127,
	    /*PAR_AUDIO_OUT1*/ 		DTYPE_MENU | (MENU_AUDIO_OUT<<4),
	    /*PAR_AUDIO_OUT2*/ 		DTYPE_MENU | (MENU_AUDIO_OUT<<4),
	    /*PAR_AUDIO_OUT3*/ 		DTYPE_MENU | (MENU_AUDIO_OUT<<4),
	    /*PAR_AUDIO_OUT4*/ 		DTYPE_MENU | (MENU_AUDIO_OUT<<4),
	    /*PAR_AUDIO_OUT5*/ 		DTYPE_MENU | (MENU_AUDIO_OUT<<4),
	    /*PAR_AUDIO_OUT6*/ 		DTYPE_MENU | (MENU_AUDIO_OUT<<4),
	    /*PAR_MIDI_NOTE1*/		DTYPE_NOTE_NAME,
	    /*PAR_MIDI_NOTE2*/		DTYPE_NOTE_NAME,
	    /*PAR_MIDI_NOTE3*/		DTYPE_NOTE_NAME,
	    /*PAR_MIDI_NOTE4*/		DTYPE_NOTE_NAME,
	    /*PAR_MIDI_NOTE5*/		DTYPE_NOTE_NAME,
	    /*PAR_MIDI_NOTE6*/		DTYPE_NOTE_NAME,
	    /*PAR_MIDI_NOTE7*/		DTYPE_NOTE_NAME,
	    /*PAR_ROLL*/ 			DTYPE_MENU | (MENU_ROLL_RATES<<4),
	    /*PAR_MORPH*/ 			DTYPE_0B255,
	    /*PAR_ACTIVE_STEP */ 	DTYPE_0B127,
	    /*PAR_STEP_VOLUME*/ 	DTYPE_0B127,
	    /*PAR_STEP_PROB*/ 		DTYPE_0B127,
	    /*PAR_STEP_NOTE*/ 		DTYPE_NOTE_NAME,
	    /*PAR_EUKLID_LENGTH*/ 	DTYPE_1B16,
	    /*PAR_EUKLID_STEPS*/ 	DTYPE_1B16,
	    /*PAR_AUTOM_TRACK*/ 	DTYPE_0b1,
	    /*PAR_P1_DEST*/ 		DTYPE_AUTOM_TARGET,
	    /*PAR_P2_DEST*/ 		DTYPE_AUTOM_TARGET,
	    /*PAR_P1_VAL*/ 			DTYPE_0B127,
	    /*PAR_P2_VAL*/ 			DTYPE_0B127,
	    /*PAR_SHUFFLE*/ 		DTYPE_0B127,
	    /*PAR_PATTERN_BEAT*/ 	DTYPE_0B127,
	    /*PAR_PATTERN_NEXT*/ 	DTYPE_MENU | (MENU_NEXT_PATTERN<<4),
	    /*PAR_TRACK_LENGTH*/ 	DTYPE_1B16,
	    /*PAR_POS_X*/ 			DTYPE_0B127,
	    /*PAR_POS_Y*/ 			DTYPE_0B127,
	    /*PAR_FLUX*/ 			DTYPE_0B127,
	    /*PAR_SOM_FREQ*/ 		DTYPE_0B127,
	    /*PAR_BPM*/ 			DTYPE_0B255,
	    /*PAR_MIDI_CHAN_1*/ 	DTYPE_1B16,
	    /*PAR_MIDI_CHAN_2*/ 	DTYPE_1B16,
	    /*PAR_MIDI_CHAN_3*/ 	DTYPE_1B16,
	    /*PAR_MIDI_CHAN_4*/ 	DTYPE_1B16,
	    /*PAR_MIDI_CHAN_5*/ 	DTYPE_1B16,
	    /*PAR_MIDI_CHAN_6*/ 	DTYPE_1B16,
	    /*PAR_FETCH*/ 			DTYPE_ON_OFF,
	    /*PAR_FOLLOW*/ 			DTYPE_ON_OFF,
	    /*PAR_QUANTISATION*/ 	DTYPE_MENU | (MENU_SEQ_QUANT<<4),
	    /*PAR_SCREENSAVER_ON_OFF*/ DTYPE_ON_OFF,
	    /*PAR_MIDI_MODE*/ 		DTYPE_MENU | (MENU_MIDI<<4),
	    /*PAR_MIDI_CHAN_7*/ 	DTYPE_1B16,
};



//-----------------------------------------------------------------
/** array holding all the available parameter values*/
uint8_t parameter_values[NUM_PARAMS];
uint8_t parameters2[END_OF_SOUND_PARAMETERS];/**< a second array for sound x-fade to another preset*/
//-----------------------------------------------------------------
//lock all 4 potentiometer values
void lockPotentiometerFetch()
{
	if(parameterFetch & PARAMETER_LOCK_ACTIVE)
	{
		//all 4 parameters locked
		parameterFetch |= 0x0F;
	}	
}
//-----------------------------------------------------------------
//takes the target  number and converts it to a parameter number
// TODO the parameter numbers are now 16 bit. We need to create a set of sensible mod targets
// (that fits in 8 bits), and have a mapping
uint8_t getModTargetValue(uint8_t value, uint8_t voiceNr)
{
	if(voiceNr >= 6) voiceNr = 5;
	// right 3 bits is active parameter, left 5 bits is page number
	const uint8_t page				= (value&MASK_PAGE)>>PAGE_SHIFT;
	const uint8_t activeParameter	= value&MASK_PARAMETER;

	return (uint8_t)pgm_read_word(&menuPages[voiceNr][page].bot1 + activeParameter);
}
//-----------------------------------------------------------------
void menu_init()
{
	lcd_clear();

	cc2Name_init();

	memset(menu_currentPresetNr,0,sizeof(uint8_t) *NUM_PRESET_LOCATIONS );

	memset(parameter_values, 0, sizeof(uint8_t) * NUM_PARAMS);

	parameter_values[PAR_EUKLID_LENGTH] = 16;
	parameter_values[PAR_EUKLID_STEPS] = 16;

	//initialize the roll value
	parameter_values[PAR_ROLL] = 8;
	//frontPanel_sendData(SEQ_CC,SEQ_ROLL_RATE,8); //value is initialized in cortex firmware

	parameter_values[PAR_BPM] = 120;

	//set voice 1 active
	menu_switchPage(0);
	frontPanel_sendData(SEQ_CC,SEQ_SET_ACTIVE_TRACK,0);
	//the currently active button is lit
	led_setActiveVoice(0);

	//display start menu page
	menu_repaintAll();

}
//-----------------------------------------------------------------
/** compare the currentDisplayBuffer with the editDisplayBuffer and send all differences to the display*/
void sendDisplayBuffer()
{
	for(uint8_t i=0;i<2;i++)
	{
		for(uint8_t j=0;j<16;j++)
		{
			if(currentDisplayBuffer[i][j] != editDisplayBuffer[i][j])
			{
				//something has changed

				//we have to omit the zero termination sign
				if(editDisplayBuffer[i][j] == '\0')
				{
					//make a clear space out of zero termination
					editDisplayBuffer[i][j] = ' ';
				}

				//set cursor to current position
				lcd_setcursor(j,(uint8_t)(i+1));
				//send the new character
				lcd_data(editDisplayBuffer[i][j]);
				//update currentDisplayBuffer
				currentDisplayBuffer[i][j] = editDisplayBuffer[i][j];
			}	
		}

	}

}
//-----------------------------------------------------------------
void menu_repaintAll()
{

	//if(copyClear_Mode == MODE_NONE)
	{
		memset(editDisplayBuffer,' ',2*17);	
		memset(currentDisplayBuffer,127,2*16);

		menu_repaint();
	}		
}
//-----------------------------------------------------------------
static void menu_repaintLoadSavePage()
{


	if(menu_activePage == SAVE_PAGE)
	{
		//Top row 
		strcpy_P(&editDisplayBuffer[0][0],PSTR("Save:"));

		switch(menu_saveOptions.what)
		{
		case WHAT_KIT:
			strcpy_P(&editDisplayBuffer[0][6],PSTR("Kit     "));
			if(menu_saveOptions.state == SAVE_STATE_EDIT_TYPE) {
				if(editModeActive) {
					editDisplayBuffer[0][5]='[';
					editDisplayBuffer[0][11]=']';
				} else {
					//arrow before parameter
					editDisplayBuffer[0][5]= ARROW_SIGN;
				}
			}	
			break;

		case WHAT_PATTERN:
		{
			strcpy_P(&editDisplayBuffer[0][6],PSTR("Pattern "));
			if(menu_saveOptions.state == SAVE_STATE_EDIT_TYPE) {
				if(editModeActive) {
					editDisplayBuffer[0][5]='[';
					editDisplayBuffer[0][13]=']';
				} else {
					//arrow before parameter
					editDisplayBuffer[0][5]= ARROW_SIGN;
				}
			}
		}
		break;

		case WHAT_MORPH:
		{
			strcpy_P(&editDisplayBuffer[0][6],PSTR("MorphKit"));
			if(menu_saveOptions.state == SAVE_STATE_EDIT_TYPE)
			{
				if(editModeActive) {
					editDisplayBuffer[0][5]='[';
					editDisplayBuffer[0][14]=']';
				} else {
					//arrow before parameter
					editDisplayBuffer[0][5]= ARROW_SIGN;
				}
			}
		}
		break;

		case WHAT_GLO:
		{
			strcpy_P(&editDisplayBuffer[0][6],PSTR("Settings"));
			if(menu_saveOptions.state == SAVE_STATE_EDIT_TYPE)
			{
				if(editModeActive) {
					editDisplayBuffer[0][5]='[';
					editDisplayBuffer[0][14]=']';
				} else {
					//arrow before parameter
					editDisplayBuffer[0][5]= ARROW_SIGN;
				}
			}
		}
		break;
		}

		//Bottom row 

		if( menu_saveOptions.what != WHAT_GLO) //no mane and number for global settings
		{
			//the preset number
			numtostrpu(&editDisplayBuffer[1][1],menu_currentPresetNr[menu_saveOptions.what]);

			if(menu_saveOptions.state == SAVE_STATE_EDIT_PRESET_NR)
			{
				if(editModeActive) {
					editDisplayBuffer[1][0]='[';
					editDisplayBuffer[1][4]=']';
				} else {
					//arrow before parameter
					editDisplayBuffer[1][0]= ARROW_SIGN;
				}
			}

			if( (menu_saveOptions.state >= SAVE_STATE_EDIT_NAME1) && (menu_saveOptions.state <= SAVE_STATE_EDIT_NAME8) )
			{			
				//write name up to active char
				memcpy(&editDisplayBuffer[1][5],(const void*)preset_currentName,8);
				if(editModeActive) {
					editDisplayBuffer[1][4+menu_saveOptions.state-SAVE_STATE_EDIT_NAME1]='[';
					editDisplayBuffer[1][6+menu_saveOptions.state-SAVE_STATE_EDIT_NAME1]=']';	
				} else {
					//arrow before parameter
					editDisplayBuffer[1][4+menu_saveOptions.state-SAVE_STATE_EDIT_NAME1]= ARROW_SIGN;
				}
			}
			else
			{
				memcpy(&editDisplayBuffer[1][5],(const void*)preset_currentName,8);
			}
		}			

		//ok button
		memcpy_P(&editDisplayBuffer[1][14],PSTR("ok"),2);
		if((menu_saveOptions.state==SAVE_STATE_OK) ||(menu_saveOptions.what == WHAT_GLO && menu_saveOptions.state > SAVE_STATE_EDIT_TYPE))
		{	
			{
				//arrow before parameter
				editDisplayBuffer[1][13]= ARROW_SIGN;
			}
		}	


	}
	//-------------- Load Page ----------------------------------------
	else
	{
		//Top row 
		strcpy_P(&editDisplayBuffer[0][0],PSTR("Load:"));

		switch(menu_saveOptions.what)
		{
		case WHAT_PATTERN:
		{
			strcpy_P(&editDisplayBuffer[0][6],PSTR("Pattern"));
			if(menu_saveOptions.state == SAVE_STATE_EDIT_TYPE)
			{
				if(editModeActive)
				{
					editDisplayBuffer[0][5]='[';
					editDisplayBuffer[0][13]=']';
				}
				else
				{
					//arrow before parameter
					editDisplayBuffer[0][5]= ARROW_SIGN;
				}

			}
		}
		break;

		case WHAT_KIT:
		{
			strcpy_P(&editDisplayBuffer[0][6],PSTR("Kit    "));
			if(menu_saveOptions.state == SAVE_STATE_EDIT_TYPE)
			{
				if(editModeActive)
				{
					editDisplayBuffer[0][5]='[';
					editDisplayBuffer[0][11]=']';
				}
				else
				{
					//arrow before parameter
					editDisplayBuffer[0][5]= ARROW_SIGN;
				}
			}
		}
		break;

		case WHAT_MORPH:
		{
			strcpy_P(&editDisplayBuffer[0][6],PSTR("MorphKit"));
			if(menu_saveOptions.state == SAVE_STATE_EDIT_TYPE)
			{
				if(editModeActive)
				{
					editDisplayBuffer[0][5]='[';
					editDisplayBuffer[0][14]=']';
				}
				else
				{
					//arrow before parameter
					editDisplayBuffer[0][5]= ARROW_SIGN;
				}

			}
		}
		break;

		case WHAT_GLO:
		{
			strcpy_P(&editDisplayBuffer[0][6],PSTR("Settings"));
			if(menu_saveOptions.state == SAVE_STATE_EDIT_TYPE)
			{
				if(editModeActive) {
					editDisplayBuffer[0][5]='[';
					editDisplayBuffer[0][14]=']';
				} else {
					//arrow before parameter
					editDisplayBuffer[0][5]= ARROW_SIGN;
				}
			}
		}
		break;
		}				

		//############ Bottom row  ############
		//the preset number

		if( menu_saveOptions.what != WHAT_GLO) //no mane and number for global settings
		{
			numtostrpu(&editDisplayBuffer[1][1], menu_currentPresetNr[menu_saveOptions.what]);

			if(menu_saveOptions.state == SAVE_STATE_EDIT_PRESET_NR)
			{
				if(editModeActive)
				{
					editDisplayBuffer[1][0]='[';
					editDisplayBuffer[1][4]=']';
				}
				else
				{
					//arrow before parameter
					editDisplayBuffer[1][0]= ARROW_SIGN;
				}
			}

			//the preset name
			memcpy(&editDisplayBuffer[1][5],(const void*)preset_currentName,8);
		}		



		//ok button for pattern page
		if(menu_saveOptions.what == WHAT_PATTERN || menu_saveOptions.what == WHAT_GLO) {
			memcpy_P(&editDisplayBuffer[1][14],PSTR("ok"),2);
			if((menu_saveOptions.state==SAVE_STATE_OK) ||
					(menu_saveOptions.what == WHAT_GLO && menu_saveOptions.state > SAVE_STATE_EDIT_TYPE)) {
				//arrow before parameter
				editDisplayBuffer[1][13]= ARROW_SIGN;
			}	
		} else { //clear ok text
			editDisplayBuffer[1][14] = 0;
			editDisplayBuffer[1][15] = 0;
		}			
	}	
}
//-----------------------------------------------------------------
uint8_t has2ndPage(uint8_t menuPage)
{
	const uint8_t textType = pgm_read_byte(&menuPages[menu_activePage][menuPage].top1 + 4);
	if(textType != TEXT_EMPTY)
	{
		return 1;
	}
	else return 0;	

}
//-----------------------------------------------------------------
uint8_t checkScrollSign(uint8_t activePage, uint8_t activeParameter)
{
	const uint8_t is2ndPage = (activeParameter>3);
	//const uint8_t textType = pgm_read_byte(&menuPages[menu_activePage][activePage].top1 + 4);
	if(has2ndPage(activePage))
	{
		return is2ndPage?'<':'>';
	}
	else return 0;	
}

//-----------------------------------------------------------------
// paint the screen when in normal parameter edit mode
// (as opposed to repainting the load save screen)
void menu_repaintGeneric()
{
	//first get the active sub-page and parameter from the menuIndex
	const uint8_t activeParameter	= menuIndex & MASK_PARAMETER; // this will be 0 to 7 for the 4 columns
	const uint8_t activePage		= (menuIndex&MASK_PAGE)>>PAGE_SHIFT; // this is the active sub page
	uint8_t curParmVal;

	// the active page
	const Page *ap=&menuPages[menu_activePage][activePage];

	if(editModeActive) //show single parameter with full name
	{
		//get address from top1-4 from activeParameter (base adress top1 + offset)
		uint8_t parName = pgm_read_byte(&ap->top1 + activeParameter);
		uint16_t parNr = pgm_read_word(&ap->bot1 + activeParameter);
		curParmVal = parameter_values[parNr];

		if(pgm_read_byte(parameter_dtypes[parNr]) == DTYPE_AUTOM_TARGET)
		{
			// make sure we have a valid value (autom target must target one of the sound parameters)
			if(curParmVal >= END_OF_SOUND_PARAMETERS)
				parameter_values[parNr] = curParmVal = END_OF_SOUND_PARAMETERS-1;

			//Top row (which destination (0 or 1) and which voice it's targeting)
			memcpy_P(&editDisplayBuffer[0][0],PSTR("Autom.Dest.    V"),16);
			numtostrpu(&editDisplayBuffer[0][11],(uint8_t)( parNr - PAR_P1_DEST));
			numtostru(&editDisplayBuffer[0][16], (uint8_t)(menu_cc2name[curParmVal].voiceNr+1));

			memset(&editDisplayBuffer[1][0],' ',16);
			// bottom row is the category and long name for the parameter being targeted
			if( (menu_cc2name[curParmVal].nameIdx < NUM_NAMES) )
			{
				char tmp[17];

				strlcpy_P(tmp,
					(const char PROGMEM *)(&catNames[pgm_read_byte(&valueNames[menu_cc2name[curParmVal].nameIdx].category)]),8);
				strcat_P(tmp,
					(const char PROGMEM *)(&longNames[pgm_read_byte(&valueNames[menu_cc2name[curParmVal].nameIdx].longName)]));
				strlcpy(&editDisplayBuffer[1][0],tmp,16);
			}

		} // parameter type is automation target

		else // parameter type is not automation target
		{
			//Top row -> category
			strcpy_P(&editDisplayBuffer[0][0],
				(const char PROGMEM *)(&catNames[pgm_read_byte(&valueNames[parName].category)]));
			//Bottom row left -> long name
			strcpy_P(&editDisplayBuffer[1][0],
				(const char PROGMEM *)(&longNames[pgm_read_byte(&valueNames[parName].longName)]));
			//Bottom row right -> parameter value (set below)

			switch(pgm_read_byte(&parameter_dtypes[parNr]) & 0x0F)
			{
			case DTYPE_TARGET_SELECTION_VELO: //switch(parameters_dtypes[parNr] & 0x0F)
			{
				const uint8_t voiceNr			= (uint8_t)(parNr - PAR_VEL_DEST_1);
				const uint8_t page				= (curParmVal & MASK_PAGE)>>PAGE_SHIFT;
				const uint8_t activeParameter	= curParmVal & MASK_PARAMETER;

				memcpy_P(&editDisplayBuffer[1][13],
					&shortNames[
					    pgm_read_byte(&valueNames[
					        pgm_read_byte(&menuPages[voiceNr][page].top1 + activeParameter)
					                             ].shortName)
					           ],3);
			}
			break;
			case DTYPE_TARGET_SELECTION_LFO: //switch(parameters_dtypes[parNr] & 0x0F)
			{
				const uint8_t lfoNr				= (uint8_t)(parNr - PAR_TARGET_LFO1);
				const uint8_t voiceNr			= (uint8_t)(parameter_values[PAR_VOICE_LFO1+lfoNr]-1);
				const uint8_t page				= (curParmVal & MASK_PAGE)>>PAGE_SHIFT;
				const uint8_t activeParameter	= curParmVal & MASK_PARAMETER;

				memcpy_P(&editDisplayBuffer[1][13],
					&shortNames[
					    pgm_read_byte(&valueNames[
					        pgm_read_byte(&menuPages[voiceNr][page].top1 + activeParameter)
					                             ].shortName)
					           ],3);
			}
			break;

			case DTYPE_MIX_FM: //switch(parameters_dtypes[parNr] & 0x0F)
				if(curParmVal == 1)
				{
					memcpy_P(&editDisplayBuffer[1][13],PSTR("Mix"), 3);
				}
				else
				{
					memcpy_P(&editDisplayBuffer[1][13],PSTR("FM"),2);
				}
				break;
			case DTYPE_ON_OFF: //switch(parameters_dtypes[parNr] & 0x0F)
				if(curParmVal == 1)
				{
					memcpy_P(&editDisplayBuffer[1][13],PSTR("On"),2);
				}
				else
				{
					memcpy_P(&editDisplayBuffer[1][13],PSTR("Off"),3);
				}
				break;

			case DTYPE_MENU: //switch(parameters_dtypes[parNr] & 0x0F)
			{
				//get the used menu (upper 4 bit)
				const uint8_t menuId = (pgm_read_byte(&parameter_dtypes[parNr]) >> 4);
				switch(menuId)
				{
				case MENU_TRANS:
					memcpy_P(&editDisplayBuffer[1][13],&transientNames[curParmVal + 1],3);
					break;

				case MENU_AUDIO_OUT:
					memcpy_P(&editDisplayBuffer[1][13],&outputNames[curParmVal + 1],3);
					break;

				case MENU_FILTER:
					memcpy_P(&editDisplayBuffer[1][13],&filterTypes[curParmVal + 1],3);
					break;

				case MENU_WAVEFORM:
					memcpy_P(&editDisplayBuffer[1][13],&waveformNames[curParmVal + 1],3);
					break;

				case MENU_SYNC_RATES:
					numtostrpu(&editDisplayBuffer[1][13],curParmVal);
					break;

				case MENU_LFO_WAVES:
					memcpy_P(&editDisplayBuffer[1][13],&lfoWaveNames[curParmVal+1],3);
					break;

				case MENU_RETRIGGER:
					memcpy_P(&editDisplayBuffer[1][13],&retriggerNames[curParmVal+1],3);
					break;

				case MENU_SEQ_QUANT:
					memcpy_P(&editDisplayBuffer[1][13],&quantisationNames[curParmVal+1],3);
					break;

				case MENU_MIDI:
					memcpy_P(&editDisplayBuffer[1][13],&midiModes[curParmVal+1],3);
					break;

				case MENU_NEXT_PATTERN:
					memcpy_P(&editDisplayBuffer[1][13],&nextPatternNames[curParmVal+1],3);
					break;


				case MENU_ROLL_RATES:
					memcpy_P(&editDisplayBuffer[1][13],&rollRateNames[curParmVal+1],3);
					break;

				default:
					memcpy_P(&editDisplayBuffer[1][13],PSTR("---"),3);
					break;
				}

			} // switch(parameters[parNr].dtype&0x0F) case DTYPE_MENU
			break;

			case DTYPE_PM63: //switch(parameters_dtypes[parNr] & 0x0F)
				// -63 to +63
				numtostrps(&editDisplayBuffer[1][13],(int8_t)(curParmVal - 63));
				break;
				//--AS note names
			case DTYPE_NOTE_NAME: //switch(parameters_dtypes[parNr] & 0x0F)
				setNoteName(curParmVal, &editDisplayBuffer[1][13]);
				break;

			default: //switch(parameters_dtypes[parNr] & 0x0F)
				// unsigned numeric value: just print the value
			case DTYPE_0B127:
			case DTYPE_0B255:
			case DTYPE_1B16:
			case DTYPE_VOICE_LFO:
			case DTYPE_0b1:
				numtostrpu(&editDisplayBuffer[1][13],curParmVal);
				break;
			} //switch(parameters_dtypes[parNr] & 0x0F) end

		} // parameter type is not automation target

	} // if editmode active
	else
	{ // editmode not active - show regular menu parameters
		//check if parameters 1-4 or 5-8 are shown
		const uint8_t is2ndPage = (activeParameter>3) * 4;
		//paint the page
		// top texts
		memcpy_P(&editDisplayBuffer[0][0],
			&shortNames[pgm_read_byte(&valueNames[pgm_read_byte(&ap->top1+is2ndPage)].shortName)],3);
		memcpy_P(&editDisplayBuffer[0][4],
			&shortNames[pgm_read_byte(&valueNames[pgm_read_byte(&ap->top2+is2ndPage)].shortName)],3);
		memcpy_P(&editDisplayBuffer[0][8],
			&shortNames[pgm_read_byte(&valueNames[pgm_read_byte(&ap->top3+is2ndPage)].shortName)],3);
		memcpy_P(&editDisplayBuffer[0][12],
			&shortNames[pgm_read_byte(&valueNames[pgm_read_byte(&ap->top4+is2ndPage)].shortName)],3);

		// capitalize selected item
		upr_three(&editDisplayBuffer[0][(activeParameter%4)*4]);

		//check if scroll sign needs to be shown
		uint8_t showScrollSign = checkScrollSign(activePage, activeParameter);
		editDisplayBuffer[0][15] = showScrollSign;


		//------------------ bottom values ----------------------------------

		// some place to store the parameter value as text
		char valueAsText[3];

		for(uint8_t i=0;i<4;i++)
		{
			const uint16_t parNr = pgm_read_word(&ap->bot1 + i +is2ndPage);
			curParmVal = parameter_values[parNr];

			//convert the parameter uint8_t value to a 3 place char
			if(parNr==PAR_NONE) {
				memcpy_P(valueAsText,PSTR("   "),3);
			} else { //not if(parNr==PAR_NONE)

				switch(pgm_read_byte(&parameter_dtypes[parNr]) & 0x0F) {
				case DTYPE_TARGET_SELECTION_VELO: //switch(parameters[parNr].dtype&0x0F)
				{
					const uint8_t voiceNr			= (uint8_t)(parNr - PAR_VEL_DEST_1);
					const uint8_t page				= (curParmVal & MASK_PAGE)>>PAGE_SHIFT;
					const uint8_t activeParameter	= curParmVal & MASK_PARAMETER;

					memcpy_P(&valueAsText,
						&shortNames[
						    pgm_read_byte(&valueNames[
						        pgm_read_byte(&menuPages[voiceNr][page].top1 + activeParameter)
						                             ].shortName)
						           ],3);
				}
				break;

				case DTYPE_TARGET_SELECTION_LFO: //switch(parameters[parNr].dtype&0x0F)
				{
					const uint8_t lfoNr				= (uint8_t)(parNr - PAR_TARGET_LFO1);
					const uint8_t voiceNr			= (uint8_t)(parameter_values[PAR_VOICE_LFO1+lfoNr] - 1);
					uint8_t page					= (curParmVal & MASK_PAGE)>>PAGE_SHIFT;
					const uint8_t activeParameter	= curParmVal & MASK_PARAMETER;

					memcpy_P(&valueAsText,
						&shortNames[
						    pgm_read_byte(&valueNames[
						        pgm_read_byte(&menuPages[voiceNr][page].top1 + activeParameter)
						                             ].shortName)
						           ],3);
				}
				break;

				case DTYPE_PM63: //switch(parameters[parNr].dtype&0x0F)
					// -63 to 64
					numtostrps(valueAsText,(int8_t)(curParmVal - 63));
					break;
					//--AS note names
				case DTYPE_NOTE_NAME: //switch(parameters[parNr].dtype&0x0F)
					setNoteName(curParmVal, valueAsText);
					break;

				case DTYPE_AUTOM_TARGET: { //switch(parameters[parNr].dtype&0x0F)
					if(curParmVal == 0xff)
					{
						memcpy_P(valueAsText,PSTR("Off"),3);
					} else {

						//const uint8_t voice = menu_cc2name[parameters[parNr].value].voiceNr;
						const uint8_t name = menu_cc2name[curParmVal].nameIdx;
						memcpy_P(&valueAsText,shortNames[pgm_read_byte(&valueNames[name].shortName)],3);

					}
				}
				break;

				case DTYPE_MIX_FM: //switch(parameters[parNr].dtype&0x0F)
					if(curParmVal == 1)
						memcpy_P(valueAsText,PSTR("Mix"),3);
					else
						memcpy_P(valueAsText,PSTR("FM "),3);
					break;
				case DTYPE_ON_OFF: //switch(parameters[parNr].dtype&0x0F)
					if(curParmVal == 1)
						memcpy_P(valueAsText,PSTR("On "),3);
					else
						memcpy_P(valueAsText,PSTR("Off"),3);
					break;
				case DTYPE_MENU: //switch(parameters[parNr].dtype&0x0F)
				{
					//get the used menu (upper 4 bit)
					const uint8_t menuId = pgm_read_byte(&parameter_dtypes[parNr]) >> 4;
					switch(menuId)
					{

					case MENU_TRANS:
						memcpy_P(&valueAsText,transientNames[curParmVal + 1],3);
						break;

					case MENU_AUDIO_OUT:
						memcpy_P(&valueAsText,outputNames[curParmVal + 1],3);
						break;

					case MENU_FILTER:
						memcpy_P(&valueAsText,filterTypes[curParmVal + 1],3);
						break;

					case MENU_WAVEFORM:

						memcpy_P(&valueAsText,waveformNames[curParmVal + 1],3);

						break;

					case MENU_SYNC_RATES:
						memcpy_P(&valueAsText,syncRateNames[curParmVal + 1],3);

						break;

					case MENU_LFO_WAVES:
						memcpy_P(&valueAsText,lfoWaveNames[curParmVal + 1],3);
						break;
					case MENU_RETRIGGER:
						memcpy_P(&valueAsText,retriggerNames[curParmVal + 1],3);
						break;

					case MENU_SEQ_QUANT:
						memcpy_P(&valueAsText,quantisationNames[curParmVal + 1],3);
						break;

					case MENU_MIDI:
						memcpy_P(&valueAsText,midiModes[curParmVal + 1],3);
						break;

					case MENU_NEXT_PATTERN:
						memcpy_P(&valueAsText,nextPatternNames[curParmVal + 1],3);
						break;

					case MENU_ROLL_RATES:
						memcpy_P(&valueAsText,rollRateNames[curParmVal + 1],3);
						break;

					default:
						memcpy_P(valueAsText,PSTR("---"),3);
						break;
					}
				} //case DTYPE_MENU: switch(parameters[parNr].dtype&0x0F)
				break;

				default: //switch(parameters[parNr].dtype&0x0F)
				case DTYPE_0B127:
				case DTYPE_0B255:
				case DTYPE_1B16:
				case DTYPE_VOICE_LFO:
				case DTYPE_0b1:
					// fallthrough for the rest of the unsigned values
					numtostrpu(valueAsText,curParmVal);
					break;
				} //switch(parameters[parNr].dtype&0x0F) end

			} // if(parNr==PAR_NONE)

			//write the text to the edit buffer
			memcpy(&editDisplayBuffer[1][4*i],valueAsText,3);

		}	// for 1 to 4 .. each menu item
	} // if editmode not active
}
//-----------------------------------------------------------------
// repaint the screen
void menu_repaint()
{

	if(menu_activePage >= LOAD_PAGE && menu_activePage<=SAVE_PAGE)
	{
		//this is a special case because the load/save page differs from all the other pages
		menu_repaintLoadSavePage();

	} else {
		menu_repaintGeneric();
	}

	//now send the changes from the edit buffer to the display
	sendDisplayBuffer();
};
//-----------------------------------------------------------------
void menu_handleSaveScreenKnobValue(uint8_t potNr, uint8_t value)
{
	uint8_t x;

	if(menu_saveOptions.state >=SAVE_STATE_EDIT_TYPE && menu_saveOptions.state <=SAVE_STATE_EDIT_PRESET_NR)
	{
		switch(potNr)
		{
		// change type
		case 0:
			x = value/(256/4); //0-4
			menu_saveOptions.what = (uint8_t)(x & 0x07); // avoid compiler warning
			switch(menu_saveOptions.what)
			{

			case WHAT_PATTERN: {
				preset_getPatternName(menu_currentPresetNr[menu_saveOptions.what]);
			}
			break;

			case WHAT_MORPH:
			case WHAT_KIT: {
				preset_getDrumsetName(menu_currentPresetNr[menu_saveOptions.what]);
			}
			break;
			}
			//force complete repaint
			menu_repaintAll();
			break;

			// change preset nr
			/*
			case 1:
				x = value/(256/127); //0-127
				menu_currentPresetNr[menu_saveOptions.what] = x;
				switch(menu_saveOptions.what)
				{

					case WHAT_PATTERN: {
						preset_getPatternName(menu_currentPresetNr[menu_saveOptions.what]);
					}
					break;

					case WHAT_SOUND: {
						preset_loadDrumset(menu_currentPresetNr[menu_saveOptions.what],0);
						preset_getDrumsetName(menu_currentPresetNr[menu_saveOptions.what]);
					}				
					break;

					case WHAT_MORPH: {
						//load to morph buffer
						preset_loadDrumset(menu_currentPresetNr[menu_saveOptions.what],1);
						preset_getDrumsetName(menu_currentPresetNr[menu_saveOptions.what]);								
					}	
					break;
				}

				menu_repaintAll();

			break;
			 */

			default:
				break;
		}			

	}
	else if(menu_saveOptions.state >=2 && menu_saveOptions.state <=9)
	{
		switch(potNr) {
		// move cursor
		case 0: //switch(potNr)
			x = value/(256/7); //0-7
			menu_saveOptions.state = (uint8_t)((2 + x) & 0x0F);
			//force complete repaint
			menu_repaintAll();
			break;

			// select lower,upper,number
		case 1://switch(potNr)
			x = value/(256/2); //0-2
			switch(x)
			{
			case 0: //Upper Case
				preset_currentName[menu_saveOptions.state - SAVE_STATE_EDIT_NAME1] =
					(char)toupper(preset_currentName[menu_saveOptions.state - SAVE_STATE_EDIT_NAME1]);
				break;

			case 1: //lower case
				preset_currentName[menu_saveOptions.state - SAVE_STATE_EDIT_NAME1] =
					(char)tolower(preset_currentName[menu_saveOptions.state - SAVE_STATE_EDIT_NAME1]);
				break;

			case 2: //numbers
				//don't know how to do this without loosing selected character
				break;

			}
			break;

			// scroll through letters
		case 2: //switch(potNr)
			    //32 to 127 => default ascii range numbers/letters
				x = (uint8_t)(value/(256/(127-32.f)) + 32); //32 - 127
				preset_currentName[menu_saveOptions.state - SAVE_STATE_EDIT_NAME1] = x;
				break;

				//nothing
		case 3: //switch(potNr)
		default://switch(potNr)
			break;
		} //switch(potNr)
	}		

}
//-----------------------------------------------------------------
void menu_handleLoadSaveMenu(int8_t inc, uint8_t button)
{
	//this is a special case because the load/save page differs from all the other pages

	//---- handle the button ----
	//pressing the button enters edit mode for the highlighted ([]) parameter
	if( (button==1) && (button!=lastEncoderButton))
	{
		//toggle edit mode
		editModeActive = (uint8_t)(1-editModeActive);
		//force complete repaint
		menu_repaintAll();

		//if the ok button is active, save/load the preset on click
		if( (editModeActive && menu_saveOptions.state == SAVE_STATE_OK) ||
			(menu_saveOptions.what == WHAT_GLO && menu_saveOptions.state > SAVE_STATE_EDIT_TYPE) ) {
			if(menu_activePage == SAVE_PAGE) {

				switch(menu_saveOptions.what) {
				case WHAT_PATTERN:
					preset_savePattern(menu_currentPresetNr[WHAT_PATTERN]);
					break;

				case WHAT_KIT:
					preset_saveDrumset(menu_currentPresetNr[WHAT_KIT],0);
					break;

				case WHAT_MORPH:
					preset_saveDrumset(menu_currentPresetNr[WHAT_MORPH],1);
					break;

				case WHAT_GLO:
					preset_saveGlobals();
					break;
				}

				menu_resetSaveParameters();						
				//editModeActive=0;
				menu_repaintAll();

			} else { //menu_activePage != SAVE_PAGE

				//load page 
				switch(menu_saveOptions.what) {

				case WHAT_PATTERN:
					if(preset_loadPattern(menu_currentPresetNr[WHAT_PATTERN])) {
						menu_resetSaveParameters();						
						//editModeActive=0;
						menu_repaintAll();
					}	
					break;

				case WHAT_GLO:
					preset_loadGlobals();
					menu_resetSaveParameters();						
					//	editModeActive=0;
					menu_repaintAll();
					break;

				default:
					break;
				}
			} // menu_activePage == or != SAVE_PAGE
		} // if ok button active
	} // (button==1) && (button!=lastEncoderButton)
	//=========================== handle encoder ===================================
	if(editModeActive) {
		//encoder changes value
		///**< 0=edit kit/pat, 1=edit preset nr, 2 = edit name*/
		switch(menu_saveOptions.state) {
		case 0: //switch(menu_saveOptions.state) - edit kit/Pat
			if(inc<0) {
				if(menu_saveOptions.what!=0) {
					menu_saveOptions.what--;
				}
			} else if(inc>0) {
				if(menu_saveOptions.what!=WHAT_GLO) {
					menu_saveOptions.what++;
				}
			}
			switch(menu_saveOptions.what) {
			case WHAT_PATTERN: {
				preset_getPatternName(menu_currentPresetNr[WHAT_PATTERN]);
			}
			break;

			case WHAT_KIT: {
				preset_getDrumsetName(menu_currentPresetNr[WHAT_KIT]);
			}
			break;

			case WHAT_MORPH: {
				preset_getDrumsetName(menu_currentPresetNr[WHAT_MORPH]);
			}
			break;

			default:
				break;
			}
			menu_repaintAll();
			break;

		case 1: //switch(menu_saveOptions.state) - edit preset nr
			if(inc<0) {
				if(menu_currentPresetNr[menu_saveOptions.what]+inc>=0)	{
					DISABLE_CONV_WARNING
					menu_currentPresetNr[menu_saveOptions.what] += inc;
					END_DISABLE_CONV_WARNING
				}
			} else if(inc>0) {
				if(menu_currentPresetNr[menu_saveOptions.what]<=125) {
					DISABLE_CONV_WARNING
					menu_currentPresetNr[menu_saveOptions.what] += inc;
					END_DISABLE_CONV_WARNING
				}
			}
			//if on load page, load the new preset when the preset number is changed
			if((inc!=0)) {
				if(menu_activePage == LOAD_PAGE) {
					switch(menu_saveOptions.what) {
					case WHAT_PATTERN: {
						preset_getPatternName(menu_currentPresetNr[menu_saveOptions.what]);
					}
					break;

					case WHAT_KIT: {
						preset_loadDrumset(menu_currentPresetNr[menu_saveOptions.what],0);
						preset_getDrumsetName(menu_currentPresetNr[menu_saveOptions.what]);
					}				
					break;

					case WHAT_MORPH: {
						//load to morph buffer
						preset_loadDrumset(menu_currentPresetNr[menu_saveOptions.what],1);
						preset_getDrumsetName(menu_currentPresetNr[menu_saveOptions.what]);
					}	
					break;
					}
				} else {
					//save page -> always load name
					preset_loadName(menu_currentPresetNr[menu_saveOptions.what], menu_saveOptions.what);
				}
			}
			break;

		//edit name
		case 2: // switch(menu_saveOptions.state) char 1
		case 3: // switch(menu_saveOptions.state) char 2
		case 4: // switch(menu_saveOptions.state) char 3
		case 5: // switch(menu_saveOptions.state) char 4
		case 6: // switch(menu_saveOptions.state) char 5
		case 7: // switch(menu_saveOptions.state) char 6
		case 8: // switch(menu_saveOptions.state) char 7
		case 9: // switch(menu_saveOptions.state) char 8
			DISABLE_CONV_WARNING
			preset_currentName[menu_saveOptions.state - SAVE_STATE_EDIT_NAME1] += inc;
			END_DISABLE_CONV_WARNING
			break;

		default: //switch(menu_saveOptions.state)
			break;
		} //switch(menu_saveOptions.state)
	} else { //editModeActive is false
		//encoder selects value to change
		if(inc<0) {
			if(menu_saveOptions.state != SAVE_STATE_EDIT_TYPE) {
				menu_saveOptions.state --;
				//no name edit on load page
				if(menu_activePage == LOAD_PAGE && menu_saveOptions.state >= SAVE_STATE_EDIT_NAME1) {
					menu_saveOptions.state = SAVE_STATE_EDIT_PRESET_NR;
				}
			}
			//force complete repaint
			menu_repaintAll();
		} else if(inc>0) {
			if(menu_saveOptions.state < SAVE_STATE_OK) {
				menu_saveOptions.state ++;

				if(menu_activePage == LOAD_PAGE)
				{
					//no name edit on load page
					if(menu_saveOptions.state >= SAVE_STATE_EDIT_NAME1 ) {
						menu_saveOptions.state = SAVE_STATE_OK;
					}
					//ok button only for load pattern
					if( (menu_saveOptions.what != WHAT_PATTERN) && (menu_saveOptions.what != WHAT_GLO)  && menu_saveOptions.state == SAVE_STATE_OK ) {
						menu_saveOptions.state = SAVE_STATE_EDIT_PRESET_NR;
					}
				}
			}

		} // inc<0
	} //editModeActive
	//force complete repaint
	menu_repaintAll();
}
//-----------------------------------------------------------------
void menu_parseEncoder(int8_t inc, uint8_t button)
{

	if(inc != 0) {
		screensaver_touch();
		//inc *= (int8_t)(-1);
		inc = (int8_t)(inc * -1);
		//limit to +/- 1
		//	inc = inc>0?1:-1;
	}		

	if(menu_activePage == LOAD_PAGE || menu_activePage == SAVE_PAGE) {
		menu_handleLoadSaveMenu(inc, button);
	} else{
		//handle the button
		if( (button==1) && (button!=lastEncoderButton)) {
			//toggle edit mode
			editModeActive = (uint8_t)(1-editModeActive);
			menu_repaintAll();
		}

		//============================= handle encoder ==============================
		if (inc!=0) {
			if(copyClear_isClearModeActive()) {
				//encoder selects clear target
				uint8_t target = copyClear_getClearTarget();
				if(inc<0) {
					if(target!=0) { 
						target--;	
					}
				} else if(inc>0) {
					if(target!=CLEAR_AUTOMATION2) {
						target++;	
					}
				}
				copyClear_setClearTarget(target);
				return;

			} // copyClear_isClearModeActive()
			else if(editModeActive) {
				//we are in edit mode
				//encoder controls parameter value
				const uint8_t activeParameter	= menuIndex & MASK_PARAMETER;
				const uint8_t activePage		= (menuIndex&MASK_PAGE)>>PAGE_SHIFT;

				//get address from top1-8 from activeParameter (base adress top1 + offset)
				uint16_t paramNr		= pgm_read_word(&menuPages[menu_activePage][activePage].bot1 + activeParameter);
				uint8_t *paramValue = &parameter_values[paramNr];

				//increase parameter value		
				if(inc>0) //positive increase
				{
					if(*paramValue != 255) //omit wrap for 0B255 dtypes
					{
						*paramValue = (uint8_t)(*paramValue + inc);
					}
				} 
				else if (inc<0) //neg increase
				{
					if(*paramValue >= abs(inc)) //omit negative wrap. inc can also be -2 or -3 depending on turn speed!
					{
						DISABLE_CONV_WARNING
						*paramValue += inc;
						END_DISABLE_CONV_WARNING
					}
				}										

				switch(pgm_read_byte(&parameter_dtypes[paramNr]) & 0x0F)
				{
				case DTYPE_TARGET_SELECTION_VELO: //parameter_dtypes[paramNr] & 0x0F
				{
					if(*paramValue > (NUM_SUB_PAGES * 8 -1))
						*paramValue = (NUM_SUB_PAGES * 8 -1); 

					uint8_t value = getModTargetValue(*paramValue, (uint8_t)(paramNr - PAR_VEL_DEST_1));

					uint8_t upper,lower;
					upper = (uint8_t)( (uint8_t)((value&0x80)>>7) |
							(((paramNr - PAR_VEL_DEST_1)&0x3f)<<1) );
					lower = value&0x7f;
					frontPanel_sendData(CC_VELO_TARGET,upper,lower);
					return;
				}
				break;

				case DTYPE_VOICE_LFO://parameter_dtypes[paramNr] & 0x0F
				{
					if(*paramValue < 1)
						*paramValue = 1;
					else if(*paramValue > 6)
						*paramValue = 6;

					uint8_t value = getModTargetValue(
							parameter_values[PAR_TARGET_LFO1+ ((uint8_t)paramNr - PAR_VOICE_LFO1)],
							(uint8_t)(*paramValue - 1));

					uint8_t upper,lower;
					upper = (uint8_t)(((value&0x80)>>7) | ((((uint8_t)(paramNr - PAR_VOICE_LFO1))&0x3f)<<1));
					lower = value&0x7f;
					frontPanel_sendData(CC_LFO_TARGET,upper,lower);
					//return;
				}
				break;
				case DTYPE_TARGET_SELECTION_LFO://parameter_dtypes[paramNr] & 0x0F
				{
					if(*paramValue > (NUM_SUB_PAGES * 8 -1))
						*paramValue = (NUM_SUB_PAGES * 8 -1);
					uint8_t voiceNr =  (uint8_t)(parameter_values[PAR_VOICE_LFO1+(paramNr - PAR_TARGET_LFO1)]-1);
					if (voiceNr == 0) voiceNr = 1;
					uint8_t value = getModTargetValue(*paramValue,(uint8_t)(voiceNr - 1));

					uint8_t upper,lower;
					upper = (uint8_t)((uint8_t)((value&0x80)>>7) | (((paramNr - PAR_TARGET_LFO1)&0x3f)<<1));
					lower = value&0x7f;
					frontPanel_sendData(CC_LFO_TARGET,upper,lower);
					//return; // --AS this was causing a failure to update
				}
				break;

				case DTYPE_AUTOM_TARGET://parameter_dtypes[paramNr] & 0x0F
					if(*paramValue >= END_OF_SOUND_PARAMETERS)
						*paramValue = END_OF_SOUND_PARAMETERS-1;
					break;

				case DTYPE_0B255:
					//if(*paramValue > 255)
					//	*paramValue = 255;
					break;


				case DTYPE_1B16://parameter_dtypes[paramNr] & 0x0F
					if(*paramValue < 1)
						*paramValue = 1;
					else if(*paramValue > 16)
						*paramValue = 16;
					break;
				case DTYPE_MIX_FM://parameter_dtypes[paramNr] & 0x0F
				case DTYPE_ON_OFF:
				case DTYPE_0b1:
					if(*paramValue > 1)
						*paramValue = 1;
					break;



				case DTYPE_MENU://parameter_dtypes[paramNr] & 0x0F
				{
					//get the used menu (upper 4 bit)
					const uint8_t menuId = pgm_read_byte(&parameter_dtypes[paramNr]) >> 4;
					//get the number of entries
					uint8_t numEntries;
					switch(menuId)
					{
					case MENU_TRANS:
						numEntries = transientNames[0][0];
						break;

					case MENU_AUDIO_OUT:
						numEntries = outputNames[0][0];

						break;

					case MENU_FILTER:
						numEntries = filterTypes[0][0];
						break;

					case MENU_SYNC_RATES:
						numEntries = syncRateNames[0][0];
						break;

					case MENU_LFO_WAVES:
						numEntries = lfoWaveNames[0][0];
						break;

					case MENU_RETRIGGER:
						numEntries = retriggerNames[0][0];
						break;		

					case MENU_SEQ_QUANT:
						numEntries = quantisationNames[0][0];
						break;

					case MENU_MIDI:
						numEntries = midiModes[0][0];
						break;

					case MENU_NEXT_PATTERN:
						numEntries = nextPatternNames[0][0];
						break;

					case MENU_WAVEFORM:
						numEntries = waveformNames[0][0];
						//value = 127*frac; //TODO to adapt full midi cc range <-> leads to inconsitencies
						break;

					case MENU_ROLL_RATES:
						numEntries = rollRateNames[0][0];
						break;

					default:
						numEntries = 0;
						break;
					}

					if(*paramValue >= numEntries)
						*paramValue = (uint8_t)(numEntries-1);

				}
				break;
				default://parameter_dtypes[paramNr] & 0x0F
				case DTYPE_0B127:
					if(*paramValue > 127)
						*paramValue = 127;
					break;
				} //parameter_dtypes[paramNr] & 0x0F

				//send parameter change to uart tx
				if(paramNr < 128) // => Sound Parameter
				{
					frontPanel_sendData(MIDI_CC,(uint8_t)paramNr,*paramValue);
				}
				// TODO how is this affected by my changes in the param enum?
				else if(paramNr>=128 && (paramNr < END_OF_SOUND_PARAMETERS)) // => Sound Parameter above 127
				{
					frontPanel_sendData(CC_2,(uint8_t)(paramNr-128),*paramValue);
				}
				else
				{
					menu_parseGlobalParam(paramNr,parameter_values[paramNr]);
				}

				//frontPanel_sendData(0xb0,paramNr,*paramValue);
			} //editModeActive
			else //-------- not in edit mode --------
			{
				//encoder selects active parameter

				/*
				//check if next parameter is not empty
				//if inc is negative avoid to integer underflow (don't decrement 0)*/
				uint8_t activeParameter	= (menuIndex+inc) & MASK_PARAMETER;
				uint8_t activePage		= (uint8_t)(((menuIndex+inc)&MASK_PAGE)>>PAGE_SHIFT);

				uint8_t param = pgm_read_byte(&menuPages[menu_activePage][activePage].top1 + activeParameter);


				if(inc>0) {

					if((param == TEXT_SKIP) && (activeParameter!=0) ) 
					{
						//skip entry
						inc++;
						activeParameter	= (menuIndex+inc) & MASK_PARAMETER;
						activePage		= (uint8_t)(((menuIndex+inc)&MASK_PAGE)>>PAGE_SHIFT);
						param = pgm_read_byte(&menuPages[menu_activePage][activePage].top1 + activeParameter);

					}		
					if((param != TEXT_EMPTY) && (activeParameter!=0) ) 
					{
						if(parameterFetch & PARAMETER_LOCK_ACTIVE)
						{
							//check if parameter lock for fetch is needed
							const uint8_t currentActiveParameter	= (menuIndex) & MASK_PARAMETER;
							if( (currentActiveParameter<=3) && (activeParameter>3) )
								//if( (((menuIndex+inc)&MASK_PAGE)>>PAGE_SHIFT) != activePage)
							{
								//lock all parameters
								lockPotentiometerFetch();
							}
						}					

						//switch menu
						DISABLE_CONV_WARNING
						menuIndex += inc;
						END_DISABLE_CONV_WARNING
					}								
				}
				else
				{

					if((param == TEXT_SKIP) && (activeParameter!=0) ) 
					{
						//skip entry
						inc--;
						activeParameter	= (menuIndex+inc) & MASK_PARAMETER;
						activePage		= (uint8_t)(((menuIndex+inc)&MASK_PAGE)>>PAGE_SHIFT);
						param = pgm_read_byte(&menuPages[menu_activePage][activePage].top1 + activeParameter);

					}

					if( (menuIndex!=0) && (activeParameter != MASK_PARAMETER) )
					{


						//check if parameter lock for fetch is needed
						if(parameterFetch & PARAMETER_LOCK_ACTIVE)
						{
							const uint8_t currentActiveParameter	= (menuIndex) & MASK_PARAMETER;
							//if( (((menuIndex+inc)&MASK_PAGE)>>PAGE_SHIFT) != activePage)
							if( (currentActiveParameter>3) && (activeParameter<=3) )
							{
								//lock all parameters
								lockPotentiometerFetch();
							}
						}											

						//switch menu
						DISABLE_CONV_WARNING
						menuIndex += inc;	
						END_DISABLE_CONV_WARNING
					}				
				}										


			} //not in edit mode

			//update the button state
			menu_repaint();

		} // if inc !=0

	}		

	lastEncoderButton = button;

};
//-----------------------------------------------------------------
void menu_resetSaveParameters()
{

	//reset save params
	//	menu_saveOptions.activeChar = 0;
	if(menu_saveOptions.what == WHAT_GLO)
	{
		menu_saveOptions.state = SAVE_STATE_EDIT_TYPE;
		menu_saveOptions.what = WHAT_KIT;
	}
	else
	{
		//	menu_saveOptions.what	= WHAT_SOUND;
		editModeActive = 1;
		menu_saveOptions.state	= SAVE_STATE_EDIT_PRESET_NR;//SAVE_STATE_EDIT_TYPE;
	}		

	menu_repaintAll();

}
//-----------------------------------------------------------------
void menu_switchSubPage(uint8_t subPageNr)
{
	//lock all parameters
	lockPotentiometerFetch();

	//get current position
	uint8_t activeParameter	= menuIndex & MASK_PARAMETER;
	uint8_t activePage		= (menuIndex&MASK_PAGE)>>PAGE_SHIFT;

	if( has2ndPage(subPageNr) &&(subPageNr == activePage))
	{
		//toggle between 1st and 2nd page
		DISABLE_CONV_WARNING
		if(activeParameter>=4)
		{
			activeParameter -= 4;	
		} else {
			activeParameter +=4;
		}


		menuIndex &= ~(MASK_PARAMETER);
		menuIndex |= (activeParameter&MASK_PARAMETER);
		END_DISABLE_CONV_WARNING
	} else
	{		
		//prevent empty pages
		if(!has2ndPage(subPageNr)) 
		{
			DISABLE_CONV_WARNING
			if(activeParameter>=4)
			{
				activeParameter -= 4;	
			}
			menuIndex &= ~(MASK_PARAMETER);
			menuIndex |= (activeParameter&MASK_PARAMETER);
			END_DISABLE_CONV_WARNING

		}
		//got to selected sub page
		DISABLE_CONV_WARNING
		menuIndex &= ~(MASK_PAGE);
		menuIndex |= (subPageNr)<<PAGE_SHIFT;
		END_DISABLE_CONV_WARNING
	}		
};
//-----------------------------------------------------------------
void menu_resetActiveParameter()
{
	uint8_t activePage		= (menuIndex&MASK_PAGE)>>PAGE_SHIFT;
	if(!has2ndPage(activePage))
	{
		DISABLE_CONV_WARNING
		menuIndex &= ~MASK_PARAMETER;
		END_DISABLE_CONV_WARNING
	}		
};
//-----------------------------------------------------------------
uint8_t menu_getSubPage()
{
	return (menuIndex&MASK_PAGE)>>PAGE_SHIFT;
};
//-----------------------------------------------------------------
void menu_switchPage(uint8_t pageNr)
{


	//clear all sequencer buttons
	led_clearSequencerLeds();


	switch(pageNr)
	{

	case PERFORMANCE_PAGE:
	case PATTERN_SETTINGS_PAGE:
	case SEQ_PAGE:

		menu_activePage = pageNr;
		//leave edit mode if active
		editModeActive = 0;
		//activate the parameter lock
		lockPotentiometerFetch();
		break;

	case LOAD_PAGE:
	{
		//re-init the save page variables
		menu_resetSaveParameters();

		if((menu_activePage != LOAD_PAGE) && (menu_activePage != SAVE_PAGE))
		{
			//when coming from another page, do a complete reset and show the sound select page
			menu_resetSaveParameters();
			//menu_saveOptions.what	= WHAT_SOUND;
		}

		//if we are already on the load/save page, toggle between load/save
		if(menu_activePage == LOAD_PAGE)
		{
			menu_activePage = SAVE_PAGE;
		}
		else
		{
			menu_activePage = LOAD_PAGE;
		}

		//leave edit mode if active
		//editModeActive = 0;
		preset_loadName(menu_currentPresetNr[menu_saveOptions.what], menu_saveOptions.what);
	}
	break;

	default: //voice pages
	{
		menu_activePage = pageNr;
		if(pageNr<7) {
			menu_setActiveVoice(pageNr);
		}
		//leave edit mode if active
		editModeActive = 0;


		//activate the parameter lock
		lockPotentiometerFetch();
		//query current sequencer step states and light up the corresponding leds

		uint8_t trackNr = menu_getActiveVoice(); //max 6 => 0x6 = 0b110
		uint8_t patternNr = menu_shownPattern; //max 7 => 0x07 = 0b111
		uint8_t value = (uint8_t)((trackNr<<4) | (patternNr&0x7));
		frontPanel_sendData(LED_CC,LED_QUERY_SEQ_TRACK,value);
	}
	break;
	}		


	//re initialize the voice LEDs
	if(pageNr==PERFORMANCE_PAGE)
	{
		//light up mute leds
		buttonHandler_showMuteLEDs();
	} else
	{
		led_setActiveVoiceLeds((uint8_t)(1<<menu_getActiveVoice()));
		menu_muteModeActive = 0;
	}

	//go to 1st parameter on sub page
	menu_resetActiveParameter();

	//force complete repaint
	menu_repaintAll();
};
//-----------------------------------------------------------------
void menu_sendAllGlobals()
{
	uint16_t i;
	for(i=PAR_BEGINNING_OF_GLOBALS;(i<NUM_PARAMS);i++)
	{
		menu_parseGlobalParam(i,parameter_values[i]);
	}
};
//-----------------------------------------------------------------
void menu_parseGlobalParam(uint16_t paramNr, uint8_t value)
{
	switch(paramNr)
	{

	case PAR_MIDI_MODE:
		frontPanel_sendData(SEQ_CC,SEQ_MIDI_MODE,parameter_values[PAR_MIDI_MODE]);
		break;

	case PAR_MIDI_CHAN_7:
		paramNr -= 5; //because they are not after one another in the param list
	case PAR_MIDI_CHAN_1:
	case PAR_MIDI_CHAN_2:
	case PAR_MIDI_CHAN_3:
	case PAR_MIDI_CHAN_4:
	case PAR_MIDI_CHAN_5:
	case PAR_MIDI_CHAN_6:

	{
		uint8_t voice = (uint8_t)(paramNr - PAR_MIDI_CHAN_1);
		uint8_t channel = (uint8_t)(value-1);
		frontPanel_sendData(SEQ_CC,SEQ_MIDI_CHAN,(uint8_t)((voice<<4) | channel ));
	}
	break;

	case PAR_POS_X:
		frontPanel_sendData(SEQ_CC,SEQ_SET_ACTIVE_TRACK,menu_getActiveVoice());
		frontPanel_sendData(SEQ_CC,SEQ_POSX,value);
		break;

	case PAR_POS_Y:
		frontPanel_sendData(SEQ_CC,SEQ_SET_ACTIVE_TRACK,menu_getActiveVoice());
		frontPanel_sendData(SEQ_CC,SEQ_POSY,value);
		break;

	case PAR_FLUX:
		frontPanel_sendData(SEQ_CC,SEQ_SET_ACTIVE_TRACK,menu_getActiveVoice());
		frontPanel_sendData(SEQ_CC,SEQ_FLUX,value);
		break;

	case PAR_SOM_FREQ:
		frontPanel_sendData(SEQ_CC,SEQ_SET_ACTIVE_TRACK,menu_getActiveVoice());
		frontPanel_sendData(SEQ_CC,SEQ_SOM_FREQ,value);
		break;

	case PAR_TRACK_LENGTH:
		frontPanel_sendData(SEQ_CC,SEQ_SET_ACTIVE_TRACK,menu_getActiveVoice());
		frontPanel_sendData(SEQ_CC,SEQ_TRACK_LENGTH,value);
		break;

	case PAR_SHUFFLE:
		frontPanel_sendData(SEQ_CC,SEQ_SHUFFLE,value);
		break;

	case PAR_AUTOM_TRACK:
		frontPanel_sendData(SEQ_CC,SEQ_SET_AUTOM_TRACK,value);
		break;

	case PAR_P1_DEST: { //step range 0-127 value range 0-255!
		frontPanel_sendData(SEQ_CC, SEQ_SELECT_ACTIVE_STEP,parameter_values[PAR_ACTIVE_STEP]);
		frontPanel_sendData(SET_P1_DEST,value>>7,value&0x7f);
	}
	break;
	case PAR_P2_DEST:
		frontPanel_sendData(SEQ_CC, SEQ_SELECT_ACTIVE_STEP,parameter_values[PAR_ACTIVE_STEP]);
		frontPanel_sendData(SET_P2_DEST,value>>7,value&0x7f);
		break;

	case PAR_P1_VAL:
		frontPanel_sendData(SET_P1_VAL,parameter_values[PAR_ACTIVE_STEP],value);
		break;

	case PAR_P2_VAL:
		frontPanel_sendData(SET_P2_VAL,parameter_values[PAR_ACTIVE_STEP],value);
		break;

	case PAR_QUANTISATION:
		frontPanel_sendData(SEQ_CC,SEQ_SET_QUANT,value);
		break;

	case PAR_SCREENSAVER_ON_OFF:

		break;

	case PAR_BPM:
		frontPanel_sendData(SET_BPM,value&0x7f,(value>>7)&0x7f);
		break;
	case PAR_MORPH:
	{
		//value += (value==127)*1;
		preset_morph(value);
	}
	break;

	case PAR_FETCH:
		if(value) {
			parameterFetch |= PARAMETER_LOCK_ACTIVE;
		} else {
			//parameterFetch &= ~PARAMETER_LOCK_ACTIVE;
			parameterFetch = 0;
		}			
		break;
		/*
		case PAR_PHASE_VOICE1:
		case PAR_PHASE_VOICE2:
		case PAR_PHASE_VOICE3:
		case PAR_PHASE_VOICE4:
		case PAR_PHASE_VOICE5:
		case PAR_PHASE_VOICE6:
			frontPanel_sendData(CC_2,paramNr-PAR_BEGINNING_OF_GLOBALS,value);
		break;
		 */

	case PAR_VOICE_DECIMATION1:
	case PAR_VOICE_DECIMATION2:
	case PAR_VOICE_DECIMATION3:
	case PAR_VOICE_DECIMATION4:
	case PAR_VOICE_DECIMATION5:
	case PAR_VOICE_DECIMATION6:
	case PAR_VOICE_DECIMATION_ALL:
		//select the track nr
		frontPanel_sendData(SEQ_CC,SEQ_SET_ACTIVE_TRACK,((uint8_t)(paramNr-PAR_VOICE_DECIMATION1)));
		//send the new output channel
		frontPanel_sendData(VOICE_CC,VOICE_DECIMATION,value);
		break;

		/*
		case PAR_AUDIO_OUT1:
		case PAR_AUDIO_OUT2:
		case PAR_AUDIO_OUT3:
		case PAR_AUDIO_OUT4:
		case PAR_AUDIO_OUT5:
		case PAR_AUDIO_OUT6:
		//select the track nr
		frontPanel_sendData(SEQ_CC,SEQ_SET_ACTIVE_TRACK,paramNr-PAR_AUDIO_OUT1);
		//send the new output channel
		frontPanel_sendData(VOICE_CC,VOICE_AUDIO_OUT,value);
		break;
		 */

	case PAR_ROLL:
	{
		/*roll rates
			0 - one shot immediate trigger
			1 - 1/1
			2 - 1/2
			3 - 1/3
			4 - 1/4
			5 - 1/6
			6 - 1/8
			7 - 1/12
			8 - 1/16
			9 - 1/24
			10 - 1/32
			11 - 1/48
			12 - 1/64
			13 - 1/96
			14 - 1/128
		 */
		frontPanel_sendData(SEQ_CC,SEQ_ROLL_RATE,value);
	}
	break;




	case PAR_EUKLID_LENGTH: {
		// set the length of the currently active track
		uint8_t length = (uint8_t)(value-1); // max 16
		uint8_t pattern = menu_shownPattern; //max 7
		uint8_t msg = (uint8_t)((pattern&0x7) | (length<<3));
		//select the track nr
		frontPanel_sendData(SEQ_CC,SEQ_SET_ACTIVE_TRACK,menu_getActiveVoice());
		frontPanel_sendData(SEQ_CC,SEQ_EUKLID_LENGTH,msg);


		uint8_t steps = (uint8_t)(parameter_values[PAR_EUKLID_STEPS] - 1); // max 16
		//uint8_t pattern = menu_shownPattern; //max 7
		msg = (uint8_t)((pattern&0x7) | (steps<<3));
		frontPanel_sendData(SEQ_CC,SEQ_EUKLID_STEPS,msg);
	}
	break;

	case PAR_EUKLID_STEPS:	{
		uint8_t steps =(uint8_t)( value-1); // max 16
		uint8_t pattern = menu_shownPattern; //max 7
		uint8_t msg =(uint8_t)( (pattern&0x7) | (steps<<3));

		//select the track nr
		frontPanel_sendData(SEQ_CC,SEQ_SET_ACTIVE_TRACK,menu_getActiveVoice());



		frontPanel_sendData(SEQ_CC,SEQ_EUKLID_STEPS,msg);
	}
	break;

	case PAR_PATTERN_BEAT:
		frontPanel_sendData(SEQ_CC,SEQ_SET_PAT_BEAT,value);
		break;

	case PAR_PATTERN_NEXT:
		frontPanel_sendData(SEQ_CC,SEQ_SET_PAT_NEXT,value);
		break;



	case PAR_ACTIVE_STEP:
		frontPanel_sendData(SEQ_CC,SEQ_REQUEST_STEP_PARAMS,value);
		break;

	case PAR_STEP_PROB:
		frontPanel_sendData(SEQ_CC,SEQ_PROB,value);
		break;

	case PAR_STEP_NOTE:
		frontPanel_sendData(SEQ_CC,SEQ_NOTE,value);
		break;

	case PAR_STEP_VOLUME:
		frontPanel_sendData(SEQ_CC,SEQ_VOLUME,value);
		break;

	//--AS TODO add send for the new midi note mode (also add code in ARM)

	}
}
//-----------------------------------------------------------------
void menu_processSpecialCaseValues(uint16_t paramNr, const uint8_t *value)
{
	if(paramNr == PAR_BPM)
	{
		//*value *= 2;
		//*value+=1;
	}


	//To see the generated pattern we have to update the step view
	else if( (paramNr == PAR_EUKLID_LENGTH) || (paramNr == PAR_EUKLID_STEPS) )
	{
		//query current sequencer step states and light up the corresponding leds 
		//frontPanel_sendData(LED_CC,LED_QUERY_SEQ_TRACK,menu_activePage);
		led_clearSequencerLeds();
	}


}
//-----------------------------------------------------------------
uint8_t getDtypeValue(uint8_t value, uint16_t paramNr)
{
	const float frac = (value/255.f);

	switch(pgm_read_byte(&parameter_dtypes[paramNr]) & 0x0F)
	{
	case DTYPE_TARGET_SELECTION_VELO:
	case DTYPE_TARGET_SELECTION_LFO:
		return (uint8_t)(frac*(NUM_SUB_PAGES * 8 -1));
		break;

	default:
	case DTYPE_0B127:
		return (uint8_t)(127*frac);
		break;



	case DTYPE_AUTOM_TARGET:
	case DTYPE_0B255:
		return value;
		break;
	case DTYPE_VOICE_LFO:
		return (uint8_t)(1 + 5*frac);
		break;
	case DTYPE_1B16:
		return (uint8_t)(1 + 15*frac);
		break;

	case DTYPE_MIX_FM:
	case DTYPE_ON_OFF:
	case DTYPE_0b1:
		return frac>0.5f;
		break;
	case DTYPE_MENU:
	{
		//get the used menu (upper 4 bit)
		const uint8_t menuId = pgm_read_byte(&parameter_dtypes[paramNr]) >> 4;
		//get the number of entries
		uint8_t numEntries;
		switch(menuId)
		{
		case MENU_TRANS:
			numEntries = transientNames[0][0];
			break;

		case MENU_AUDIO_OUT:
			numEntries = outputNames[0][0];

			break;

		case MENU_FILTER:
			numEntries = filterTypes[0][0];
			break;

		case MENU_SYNC_RATES:
			numEntries = syncRateNames[0][0];
			break;

		case MENU_LFO_WAVES:
			numEntries = lfoWaveNames[0][0];
			break;
		case MENU_RETRIGGER:
			numEntries = retriggerNames[0][0];
			break;	

		case MENU_SEQ_QUANT:
			numEntries = quantisationNames[0][0];
			break;

		case MENU_MIDI:
			numEntries = midiModes[0][0];
			break;

		case MENU_NEXT_PATTERN:
			numEntries = nextPatternNames[0][0];
			break;

		case MENU_ROLL_RATES:
			numEntries = rollRateNames[0][0];
			break;

		case MENU_WAVEFORM:
			numEntries = waveformNames[0][0];
			//value = 127*frac; //TODO to adapt full midi cc range <-> leads to inconsitencies
			break;

		default:
			numEntries = 1;
			break;
		}
		return (uint8_t)(frac * (numEntries-1));

	}

	break;
	case DTYPE_PM63:
	case DTYPE_NOTE_NAME:
		return (uint8_t)(127*frac);
		break;
	}	
};
//-----------------------------------------------------------------
void menu_parseKnobValue(uint8_t potNr, uint8_t value)
{
	screensaver_touch();

	if( (menu_activePage == SAVE_PAGE) || (menu_activePage == LOAD_PAGE)) {
		menu_handleSaveScreenKnobValue(potNr, value);
	} else {
		const uint8_t activePage		= (menuIndex&MASK_PAGE)>>PAGE_SHIFT;
		const uint8_t activeParameter	= menuIndex & MASK_PARAMETER;
		const uint8_t isOn2ndPage		= ( activeParameter > 3) * 4;
		uint16_t paramNr					= pgm_read_word(&menuPages[menu_activePage][activePage].bot1 +
											potNr + isOn2ndPage);

		//parameter fetch
		const uint8_t dtypeValue = getDtypeValue(value,paramNr);
		if(parameter_values[paramNr] == dtypeValue)
		{
			//turn lock off for current pot
			DISABLE_CONV_WARNING
			parameterFetch &= ~(1<<potNr);
			END_DISABLE_CONV_WARNING
			return;
		}

		menu_processSpecialCaseValues(paramNr,&dtypeValue); 

		//if parameter lock is off
		if((parameterFetch & (1<<potNr)) == 0 )
		{

			//make changes temporary while an automation step is armed - save original value
			if((buttonHandler_resetLock==0) && buttonHandler_getArmedAutomationStep() != NO_STEP_SELECTED)
			{
				buttonHandler_originalValue = parameter_values[paramNr];
				buttonHandler_originalParameter = paramNr;
				buttonHandler_resetLock = 1;
			}				


			//update parameter value
			parameter_values[paramNr] = value = dtypeValue;

			switch(pgm_read_byte(&parameter_dtypes[paramNr]) & 0x0F)
			{
			case DTYPE_TARGET_SELECTION_VELO:
			{
				value = getModTargetValue(value,((uint8_t)(paramNr - PAR_VEL_DEST_1)));

				uint8_t upper,lower;
				upper = (uint8_t)(((value&0x80)>>7) | ((((uint8_t)(paramNr - PAR_VEL_DEST_1))&0x3f)<<1));
				lower = value&0x7f;
				frontPanel_sendData(CC_VELO_TARGET,upper,lower);
				return;
			}
			break;

			case DTYPE_VOICE_LFO:
			{


				value = getModTargetValue(parameter_values[PAR_TARGET_LFO1+
				                                           ((uint8_t)(paramNr - PAR_VOICE_LFO1))], (uint8_t)(value -1));

				uint8_t upper,lower;
				upper = (uint8_t)(((value&0x80)>>7) | ((((uint8_t)(paramNr - PAR_VOICE_LFO1))&0x3f)<<1));
				lower = value&0x7f;
				frontPanel_sendData(CC_LFO_TARGET,upper,lower);
				return;
			}
			break;
			case DTYPE_TARGET_SELECTION_LFO:
			{
				uint8_t voiceNr = (uint8_t)( parameter_values[PAR_VOICE_LFO1+((uint8_t)( paramNr - PAR_TARGET_LFO1))] - 1);
				if (voiceNr == 0) voiceNr = 1;
				value = getModTargetValue(value,(uint8_t)(voiceNr-1)); // --AS subtract 1 from voice nr ???

				uint8_t upper,lower;
				upper = (uint8_t)(((value&0x80)>>7) | ((((uint8_t)(paramNr - PAR_TARGET_LFO1))&0x3f)<<1));
				lower = value&0x7f;
				frontPanel_sendData(CC_LFO_TARGET,upper,lower);
				return;
			}
			break;

			default:
				break;
			}		



			if(paramNr<128) // => Sound Parameter
					{
				frontPanel_sendData(MIDI_CC,(uint8_t)paramNr,value);
					}
			else if(paramNr>=128 && (paramNr < END_OF_SOUND_PARAMETERS)) // => Sound Parameter above 127
			{
				frontPanel_sendData(CC_2,(uint8_t)(paramNr-128),value);
			}
			else
			{
				menu_parseGlobalParam(paramNr,parameter_values[paramNr]);
			}

		}
	}		
};
//-----------------------------------------------------------------
void menu_sendAllParameters()
{
	uint16_t i;
	for(i=0;i<END_OF_SOUND_PARAMETERS;i++)
		//for(i=PAR_PAN5;i<PAR_MOD_OSC_GAIN1_CYM;i++)

	{
		//send parameter change to uart tx
		//since we are sending a big amount of data here we have to be sure 
		//that the uart tx buffer doesn't overflow
		//so we check the return value
		if(i<128) {
			frontPanel_sendData(MIDI_CC,(uint8_t)i,parameter_values[i]);
		} else {
			frontPanel_sendData(CC_2,(uint8_t)(i-128),parameter_values[i]);
		}	
		//frontPanel_sendData(MIDI_CC,i,parameters[i].value);

		//delay to not overflow the rx buffer on the cortex
		//TODO ACK scheme for speedup testen
		if((i&0x1f) == 0x1f) //every 32 steps
			_delay_ms(1);
	}		
}
//----------------------------------------------------------------
uint8_t menu_getActivePage()
{
	return menu_activePage;
};
//----------------------------------------------------------------
uint8_t menu_getActiveVoice()
{
	return menu_activeVoice;
};
//----------------------------------------------------------------
void menu_setActiveVoice(uint8_t voiceNr)
{
	menu_activeVoice = voiceNr;
};
//----------------------------------------------------------------
uint8_t menu_areMuteLedsShown()
{
	return menu_muteModeActive;
}
//----------------------------------------------------------------

//--AS given a 0-127 number will return a midi note name
void setNoteName(uint8_t num, char *buf)
{
	uint8_t n = num%12; // get the note number (0 is C, 9 is A, 11 is B)
	// get the note letter ascii A=65, C=67. All repeat twice except E and B.
	// B is at the end, so only E presents a hurdle
	buf[0]=(char)((n>8 ? 60 : 67) + ((n+(n>4))/2));
	// determine natural or sharp (# if the note is 1, 3, 6, 8, 10)
	buf[1]=(n < 5 && n&1) || (n > 5 && !(n&1)) ? '#' : ' ';
	// determine the octave 0=48
	buf[2]=(char)(48+(num / 12));
}


/* fill up to 3 bytes of a buffer with string representation of a number */
// space padded unsigned
void numtostrpu(char *buf, uint8_t num)
{
    if(num > 99) {
        buf[0]=(char)('0'+(num / 100));
        num %= 100;
    } else {
        buf[0]=' ';
    }
    if(num > 9) {
        buf[1]=(char)('0'+(num / 10));
        num %=10;
    } else if(buf[0]==' ') {
        buf[1]=' ';
    } else
        buf[1]='0';
    buf[2]=(char)('0'+num);
}

// space padded signed
void numtostrps(char *buf, int8_t num)
{

    if(num > 99) {
        buf[0]=(char)('0'+(num / 100));
        num %= 100;
    } else if(num < 0) {
        buf[0]='-';
        num = (int8_t)(-num);
    } else {
        buf[0]=' ';
    }

    if(num > 9) {
        buf[1]=(char)('0'+(num / 10));
        num %=10;
    } else if(buf[0] < '0' /* space or minus */) {
        buf[1]=' ';
    } else {
        buf[1]='0';
    }
    buf[2]=(char)('0'+num);
}

// non space padded unsigned
void numtostru(char *buf, uint8_t num)
{
    uint8_t b=0;
    if(num > 99) {
        buf[b++]=(char)('0'+(num / 100));
        num %= 100;
        if(num < 10)
            buf[b++]='0';
    }
    if(num > 9) {
        buf[b++]=(char)('0'+(num / 10));
        num %=10;
    }
    buf[b]=(char)('0'+num);
}

// non space padded signed
void numtostrs(char *buf, int8_t num)
{
    uint8_t b=0;
    if(num > 99) {
        buf[b++]=(char)('0'+(num / 100));
        num %= 100;
        if(num < 10)
            buf[b++]='0';
    } else if(num < 0) {
        buf[b++]='-';
        num=(int8_t)(-num);
    }

    if(num > 9) {
        buf[b++]=(char)('0'+(num / 10));
        num %=10;
    }
    buf[b]=(char)('0'+num);
}

// make 1st 3 letters of buffer uppercase
void upr_three(char *buf)
{
	DISABLE_CONV_WARNING
	if(*buf >='a' && *buf <= 'z')
		(*buf) -=32;
	buf++;
	if(*buf >='a' && *buf <= 'z')
		(*buf) -=32;
	buf++;
	if(*buf >='a' && *buf <= 'z')
		(*buf) -=32;
	buf++;
	END_DISABLE_CONV_WARNING
}
