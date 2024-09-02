#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <ogcsys.h>
#include <unistd.h>
#include <sys/stat.h>
#include <gccore.h>
#include <network.h>
#include <debug.h>
#include <errno.h>
#include <wiiuse/wpad.h>
#include <ogc/lwp_watchdog.h>
#include <fat.h>
#include <map>

#include <curl/curl.h>
#include <wiisocket.h>

#include "rapidjson/document.h"
#include "lichess/lichess.hpp"
#include "libwiigui/gui.h" // including a ton of libraries like I'm a python user
#include "assets.h"
//#include "font_ttf.h"
#define SQUARE_SIZE 45
//static void *xfb = NULL;
//static GXRModeObj *rmode = NULL;
static GuiImageData * pointer[4];
void *initialise(); 
void *gameNetworker (void *arg);
void *eventStreamer (void *arg);
void *onScreenKeyboardThread (void *arg);
int materialEvaluator(char board[8][9], bool forWhite);
bool guiHalt = false;
bool guiHalted = false;
bool goToMainMenu = false;
bool gameStarted = false;
bool canMove = true;
bool inSeek = false;
bool displayBoard = true;
bool whitesTurn = true;
bool opponentGone = false;
bool unlimitedTime = false;
bool inOSK = false;
bool loginUpdate = false;
GuiWindow mainWin;
GuiText topUID("Anonymous", 20, (GXColor){255, 255, 255, 255}); // needs to be global for easy access
GuiText bottomUID("you", 20, (GXColor){255, 255, 255, 255});
GuiText topElo("3200", 16, (GXColor){255, 255, 255, 255});
GuiText bottomElo("3200", 16, (GXColor){255, 255, 255, 255});
GuiSound captureSnd(Capture_ogg, Capture_ogg_size, SOUND_OGG);
GuiSound moveSnd(Move_ogg, Move_ogg_size, SOUND_OGG);
GuiSound notifySnd(GenericNotify_ogg, GenericNotify_ogg_size, SOUND_OGG);
std::string wElo = " ";
std::string bElo = " ";
DisplayList chatLists;
GuiDisplayList* chat;
std::string debugText;
static lwp_t gameNetworker_handle = (lwp_t)NULL;
static lwp_t eventStreamer_handle = (lwp_t)NULL;
static lwp_t moveHdl = (lwp_t)NULL;
static lwp_t kbHdl = (lwp_t)NULL;
static lwp_t menuPromptHdl = (lwp_t)NULL;
static lwp_t challengePromptHdl = (lwp_t)NULL;
static lwp_t opponentGoneHdl = (lwp_t)NULL;
static lwp_t seekHdl = (lwp_t)NULL;
static lwp_t signOutHdl = (lwp_t)NULL;
static lwp_t createChallengePromptHdl = (lwp_t)NULL;
enum Pieces{
	BR = 'A', // black rook and so on [abbreviations]
	BN = 'B',
	BB = 'C',
	BQ = 'D',
	BK = 'E',
	BP = 'F',
	WR = 'G',
	WN = 'H',
	WB = 'I',
	WQ = 'J',
	WK = 'K',
	WP = 'L'
};
std::string gameId = "2nXvqBFv"; // default game id, from testing stage. it should never open to this game though
std::string username = "undefined";
bool curColor = true; // true = white, false = black
bool pieceSelected = false;
Pieces selectedType = BR;
int selectedPiece[2] = {1000, 1000};
int wtime = 1234567;
int btime = 1234567;
int lastClockUpdate = 0;
int whenCanClaimVictory = 10000000;
std::string tok;
std::map<char, GuiImageData*> pieces;
char startPos[8][9] = {
/*
 abcdefgh
*/
"GHIJKIHG", // 1
"LLLLLLLL", // 2
"________", // 3
"________", // 4
"________", // 5
"________", // 6
"FFFFFFFF", // 7
"ABCDECBA"  // 8
};
char curBoard[8][9] = {
/*
 abcdefgh
*/
"GHIJKIHG", // 1
"LLLLLLLL", // 2
"________", // 3
"________", // 4
"________", // 5
"________", // 6
"FFFFFFFF", // 7
"ABCDECBA"  // 8
};
GXColor boardColors[2] = {(GXColor){240, 217, 181, 255}, (GXColor){148, 111, 81, 255}};
void* menuRender(void* arg);
void* gameRender(void* arg);
void *loginRender (void *arg);
static void OnScreenKeyboard(char * var, u16 maxlen);
int WindowPrompt(const char *title, const char *msg, const char *btn1Label, const char *btn2Label);
int WindowPrompt4(const char *title, const char *msg, const char *btn1Label, const char *btn2Label, const char *btn3Label, const char *btn4Label);
void createChallengePrompt();
void applyMove(char boardBuf[8][9], std::string move);
void exitApp(){
	fatUnmount(0);
	exit(0);
}
static void
HaltGui()
{
	guiHalt = true;
	// wait for thread to finish
	while (!guiHalted)
		usleep(100);
}
static void
ResumeGui()
{
	guiHalt = false;
}
void initPieces(){
	pieces[BR] = new GuiImageData(br_png);
	pieces[BN] = new GuiImageData(bn_png);
	pieces[BB] = new GuiImageData(bb_png);
	pieces[BQ] = new GuiImageData(bq_png);
	pieces[BK] = new GuiImageData(bk_png);
	pieces[BP] = new GuiImageData(bp_png);
	pieces[WR] = new GuiImageData(wr_png);
	pieces[WN] = new GuiImageData(wn_png);
	pieces[WB] = new GuiImageData(wb_png);
	pieces[WQ] = new GuiImageData(wq_png);
	pieces[WK] = new GuiImageData(wk_png);
	pieces[WP] = new GuiImageData(wp_png);
}
//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	
	pointer[0] = new GuiImageData(player1_point_png);
	pointer[1] = new GuiImageData(player2_point_png);
	pointer[2] = new GuiImageData(player3_point_png);
	pointer[3] = new GuiImageData(player4_point_png);
	InitVideo();
	SetupPads();
	AUDIO_Init(NULL);
	ASND_Init();
	ASND_Pause(0);
	initPieces();
	InitFreeType((u8*)font_ttf, font_ttf_size);
	s32 wsRet = wiisocket_init();
	if(wsRet < -1){
		printf("wiisocket configuration failed!\nExiting in 3 seconds.");
		usleep(3000000);
		exitApp();
	} else {
		printf("wiisocket initialized\n");
	}
	if(curl_global_init(CURL_GLOBAL_DEFAULT)){
		printf("libcurl configuration failed!\nExiting in 3 seconds.");
		usleep(3000000);
		exitApp();
	} else {
		printf("libcurl initialized\n");
	}
	if(!fatInitDefault()){ // quite the bit of initialization isnt it
		printf("No suitable storage device was found. An SD card or a USB drive is required for Liichess. Exiting in 3 seconds...");
		usleep(3000000);
		exitApp();
	}
	/*if (!chdir("sd:/")){
		rootDirectory = "sd:/";
	} else if (!chdir("usb:/")){
		rootDirectory = "usb:/";
	} else {
		printf("No USB or SD card detected. An SD card or a USB drive is required for Liichess. Exiting in 3 seconds...");
		usleep(3000000);
		exitApp();
	}*/
	if (chdir("/LichessWii/")){
		mkdir("/LichessWii/", 0777); // if directory doesn't exist, make it
	}
	//chdir(rootDirectory.c_str());
	chdir("/");
	mainWin = GuiWindow(640, 480);
	mainWin.SetPosition(0,0);
	chatLists.fields.push_back("Please be nice in chat!");
	chatLists.fields.push_back("Chat autoscrolls.");
	chat = new GuiDisplayList(200, 248, &chatLists);
	chat->SetPosition(0, 60);
	chat->SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	if (access("/LichessWii/token.txt", F_OK) != 0){
		loginRender(NULL);
	} else {
		FILE* f = fopen("/LichessWii/token.txt", "rb");
		char c;
		while(1) {
      		c = fgetc(f);
      		if( feof(f) ) {
         		break ;
      		}
      		tok.push_back(c);
   		}
	}
	std::string profile = lichess::getProfile(tok); // profile can help us get user's username before even queueing for a game
	rapidjson::Document doc;
	doc.Parse(profile.c_str());
	username = doc["username"].GetString();
	LWP_CreateThread(	&eventStreamer_handle,	// thread handle 
						eventStreamer,			// code 
						NULL,		// arg pointer for thread 
						NULL,			// stack base 
						16*1024,		// stack size 
						100				// thread priority  
					);
	menuRender(NULL);
	while (1) {}
	fatUnmount(0);
	return 0;
}
void *loginRender(void* arg){
	GuiWindow bgWnd = GuiWindow(640, 480);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData boxOutline(keyboard_textbox_png);
	GuiImageData boxOutlineOver(keyboard_textbox_png);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImage uBtnImg(&boxOutline);
	GuiImage uBtnOverImg(&boxOutlineOver);
	GuiImage pBtnImg(&boxOutline);
	GuiImage pBtnOverImg(&boxOutlineOver);
	GuiImage loginBtnImg(&btnOutline);
	GuiImage loginBtnOverImg(&btnOutlineOver);

	GuiImageData mainbg(mainbg_png);
	GuiTrigger trigA;
	GuiImage bgImg(640, 480, (GXColor){38, 36, 33, 255});

	GuiText usernameTxt("Enter Username Here", 17, (GXColor){0, 0, 0, 255});
	GuiText passwordTxt("Enter Password Here", 17, (GXColor){0, 0, 0, 255});
	GuiText loginTxt("Sign in", 20, (GXColor){0, 0, 0, 255});

	GuiText signInTxt("Sign in to your Lichess account", 23, (GXColor){255, 255, 255, 255});
	signInTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	signInTxt.SetPosition(0, -80);

	GuiText errorTxt("There was an issue logging in. Try another username or password.", 18, (GXColor){255, 0, 0, 255});
	errorTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	errorTxt.SetPosition(0, 130);


	bgImg.SetImage(&mainbg);

	GuiButton usernameBtn(boxOutline.GetWidth(), boxOutline.GetHeight());

	usernameBtn.SetImage(&uBtnImg);
	usernameBtn.SetImageOver(&uBtnOverImg);
	usernameBtn.SetLabel(&usernameTxt);
    usernameBtn.SetTrigger(&trigA);
	usernameBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	usernameBtn.SetPosition(0, -30);
	usernameBtn.SetSoundOver(&btnSoundOver);

	GuiButton passwordBtn(boxOutline.GetWidth(), boxOutline.GetHeight());

	passwordBtn.SetImage(&pBtnImg);
	passwordBtn.SetImageOver(&pBtnOverImg);
	passwordBtn.SetLabel(&passwordTxt);
    passwordBtn.SetTrigger(&trigA);
	passwordBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	passwordBtn.SetPosition(0, 30);
	passwordBtn.SetSoundOver(&btnSoundOver);

	GuiButton loginBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	loginBtn.SetImage(&loginBtnImg);
	loginBtn.SetImageOver(&loginBtnOverImg);
	loginBtn.SetLabel(&loginTxt);
    loginBtn.SetTrigger(&trigA);
	loginBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	loginBtn.SetPosition(0, 90);
	loginBtn.SetSoundOver(&btnSoundOver);
	
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	mainWin.SetState(STATE_DISABLED);
	bgWnd.SetState(STATE_DISABLED);

	bgWnd.Append(&bgImg);
	mainWin.Append(&usernameBtn);
	mainWin.Append(&passwordBtn);
	mainWin.Append(&loginBtn);
	mainWin.Append(&signInTxt);
	mainWin.Append(&errorTxt);

	mainWin.SetState(STATE_DEFAULT);
	bgWnd.SetState(STATE_DEFAULT);
	char username[21] = "";
	char password[255] = "";
	int errorExpiry = ticks_to_millisecs(gettime());
	while (1){
		if(guiHalt)
		{
			guiHalted = true;
			while (guiHalt)
				usleep(100);
			guiHalted = false;
		} else {
			UpdatePads();
			u32 pressed = WPAD_ButtonsHeld(0);
			if ( pressed & WPAD_BUTTON_HOME ) 
				exitApp();
			bgWnd.Draw();
			mainWin.Draw();
			int curTime = ticks_to_millisecs(gettime());
			for(int i=3; i >= 0; i--) // so that player 1's cursor appears on top!
			{
				if(userInput[i].wpad->ir.valid){
					Menu_DrawImg(userInput[i].wpad->ir.x-48, userInput[i].wpad->ir.y-48,
							96, 96, pointer[i]->GetImage(), userInput[i].wpad->ir.angle, 1, 1, 255);
				}
				DoRumble(i);
			}
			Menu_Render();
			signInTxt.SetVisible(!inOSK);
			usernameBtn.SetVisible(!inOSK);
			passwordBtn.SetVisible(!inOSK);
			loginBtn.SetVisible(!inOSK);
			errorTxt.SetVisible(((curTime <= errorExpiry) && !inOSK));
			if (loginUpdate){
				usernameTxt.SetText(username);
				size_t pLen = strlen(password);
				std::string pText;
				if (pLen < 32){
					pText = std::string(pLen, '*');
				} else {
					pText = std::string(32, '*');
				}
				passwordTxt.SetText(pText.c_str());
				loginUpdate = false;
			}
			for(int i=0; i < 4; i++)
				mainWin.Update(&userInput[i]);
			if (usernameBtn.GetState() == STATE_CLICKED){
				std::pair<char*, int>* data = new std::pair<char*, int>(username, 21);
				LWP_CreateThread(	&kbHdl,	// thread handle 
					onScreenKeyboardThread,			// code 
					data,		// arg pointer for thread 
					NULL,			// stack base 
					16*1024,		// stack size 
					90				// thread priority  
				);
				usernameBtn.SetState(STATE_DEFAULT);
			}
			if (passwordBtn.GetState() == STATE_CLICKED){
				std::pair<char*, int>* data = new std::pair<char*, int>(password, 255);
				LWP_CreateThread(	&kbHdl,	// thread handle 
					onScreenKeyboardThread,			// code 
					data,		// arg pointer for thread 
					NULL,			// stack base 
					16*1024,		// stack size 
					90				// thread priority  
				);
				passwordBtn.SetState(STATE_DEFAULT);
			}
			if (loginBtn.GetState() == STATE_CLICKED){
				bool authSuccess = lichess::authenticate(username, password);
				if (authSuccess){
					FILE* f = fopen("/LichessWii/token.txt", "wb");
					tok = lichess::getPersonalToken();
					fwrite(tok.c_str(), 1, tok.size(), f);
					fclose(f);
					break;
				} else {
					unlink("/LichessWii/cookies.txt"); // empty cookie jars can cause crashes if not cleaned
					errorExpiry = curTime + 6000;
				}
				loginBtn.SetState(STATE_DEFAULT);
			}
		}
	}
	return NULL;
}
void *createChallengePromptThread(void* arg){
	createChallengePrompt();
	return NULL;
}
void *signOutPromptThread(void* arg){
	int resp = WindowPrompt("Confirm", "Are you sure you want to sign out?", "Yes", "No");
	switch (resp){
		case 1:
			unlink("/LichessWii/cookies.txt"); // delete saved login to sign out
			unlink("/LichessWii/token.txt");
			exitApp();
		case 0:
			break;
		default:					
			break;
	}
	return NULL;
}
void *onScreenKeyboardThread(void* arg){
	inOSK = true;
	std::pair<char*, int>* data = (std::pair<char*, int>*)arg;
	OnScreenKeyboard(data->first, data->second);
	loginUpdate = true;
	inOSK = false;
	delete data;
	return NULL;
}
void* makeMoveThread(void* arg){
	canMove = false; // Don't let the user make double moves or else behavior could be undesired
	std::string move = *((std::string*)arg); // argument is a pointer to a string containing the move; dereference it
	char boardBuf[8][9];
	memcpy(boardBuf, curBoard, sizeof(char) * 72); // store a backup of the board state in a buffer
	applyMove(curBoard, move);
	bool result = lichess::makeMove(gameId, move, tok);
	if (!result){ // Check if move was successful
		HaltGui();
		memcpy(curBoard, boardBuf, sizeof(char) * 72); // if move was not successful, restore board to how it was before (ie user made an illegal move and server rejected it)
		ResumeGui();
	} else {
		if (materialEvaluator(boardBuf, !curColor) != materialEvaluator(curBoard, !curColor)){ // make different sound depending on whether it was a capture or not
			captureSnd.Stop();
			captureSnd.Play();
		} else {
			moveSnd.Stop();
			moveSnd.Play();
		}
	}
	canMove = true; // operations on this move are done, so it is safe to let the user make more moves
	delete (std::string*)arg; // memory leaks are not fun and this string was created by the new keyword
	return NULL;
}
void* msgBoxThread(void* args){ // Message boxes yield the thread they are run in; giving them their own threads mitigates this
	canMove = false; // prevent user from accidentally moving pieces through the prompt
	std::string txt = *((std::string*)args);
	WindowPrompt("Notice", txt.c_str(), "Ok", "Ok");
	canMove = true;
	delete (std::string*)args; // no memory leaks :)
	return NULL;
}
void *menuPromptThread(void* args){
	canMove = false;
	int choice = WindowPrompt("Confirm", "Are you sure you want to return to the main menu? The game may be forfeited.", "Yes", "No");
	switch (choice){
		case 1:
			goToMainMenu = true;
		case 0:
			break;
		default:
			break;
	}
	canMove = true;
	return NULL;
}
void* promotionPromptThread(void* args){
	canMove = false; // Don't let the user make double moves or else behavior could be undesired (again)
	std::string move = *((std::string*)args);
	const char* lookup = "qrbn";
	int res = WindowPrompt4("Promotion", "What piece do you want to promote to", "Queen", "Rook", "Bishop", "Knight");
	move.push_back(lookup[res - 1]); // pulls the letter of the piece based on the choice
	HaltGui(); // Halt gui before applying move so that the gui thread and current thread don't access the board at the same time and cause undefined behavior
	applyMove(curBoard, move);
	ResumeGui();
	lichess::makeMove(gameId, move, tok);
	canMove = true;
	delete (std::string*)args; // no memory leaks :)
	return NULL;
}
void *chatPromptThread(void* args){ // All this does is give the user an on-screen keyboard and allow them to use it to send a chat message
	canMove = false;
	displayBoard = false;
	char buf[142] = "";
	OnScreenKeyboard(buf, 141); // limit to 140 characters (lichess chat limit)
	canMove = true;
	displayBoard = true;
	lichess::sendChat(gameId, buf, tok);
	return NULL;
}
void *challengePromptThread(void* arg){
	canMove = false;
	std::pair<std::string*, std::string*>* data = (std::pair<std::string*, std::string*>*)arg;
	int choice = WindowPrompt("Incoming Challenge", data->second->c_str(), "Yes", "No");
	switch (choice){
		case 1:
			lichess::acceptChallenge(*(data->first), tok);
			break;
		case 0:
			lichess::declineChallenge(*(data->first), tok);
			break;
		default:
			break;
	}
	delete data->first;
	delete data->second;
	delete data; // cleanup all the stuff on the heap
	canMove = true;
	return NULL;
}
void *createSeekThread(void* arg){
	inSeek = true;
	std::pair<int, int>* data = (std::pair<int, int>*)arg;
	lichess::createSeek(data->first, data->second, tok);
	delete data; // cleanup all the stuff on the heap
	inSeek = false;
	return NULL;
}
void *opponentGoneThread(void* arg){
	int choice = WindowPrompt("Claim Victory?", "Your opponent has been gone for an extended period of time. Do you want to claim victory?", "Yes", "No");
	if (choice == 1)
		lichess::claimVictory(gameId, tok);
	return NULL;
}
void createSeekExpeditor(int time, int inc){ // creates the thread
	std::pair<int, int>* arg = new std::pair<int, int>();
	arg->first = time;
	arg->second = inc;
	LWP_CreateThread(	&seekHdl,	// thread handle 
						createSeekThread,			// code 
						arg,		// arg pointer for thread 
						NULL,			// stack base 
						16*1024,		// stack size 
						80				// thread priority  
					);
}
std::string formatTime(int ms){ // very professional time formatter
	int sec = ms / 1000;
	ms %= 1000;
	int mins = sec / 60;
	sec %= 60;
	int hrs = mins / 60;
	mins %= 60;
	std::string secStr = std::to_string(sec);
	if (secStr.size() == 1){
		secStr = "0" + secStr; // if secStr = 4 for example, instead of returning :4 this will now return :04
	}
	std::string res;
	if (hrs == 0){ // if there are no hours, don't display them
		res = std::to_string(mins) + ":" + secStr; // don't use minsStr since I don't want times like "06:00" instead of "6:00"
	} else {
		std::string minsStr = std::to_string(mins);
		if (minsStr.size() == 1){
			minsStr = "0" + minsStr; // if minsStr = 5 for example, instead of returning :5 this will now return :05
		}
		res = std::to_string(hrs) + ":" + minsStr + ":" + secStr;
	}
	return res;
}
int materialEvaluator(char board[8][9], bool forWhite){
	int mat = 0;
	for (int x = 0; x < 8; x++){
		for (int y = 0; y < 8; y++){
			if (forWhite){
				switch (board[x][y]){
					case WR:
						mat += 5;
						break;
					case WQ:
						mat += 9;
						break;
					case WB:
						mat += 3;
						break;
					case WN:
						mat += 3;
						break;
					case WP:
						mat += 1;
						break;
					default:
						break;
				}
			} else {
				switch (board[x][y]){
					case BR:
						mat += 5;
						break;
					case BQ:
						mat += 9;
						break;
					case BB:
						mat += 3;
						break;
					case BN:
						mat += 3;
						break;
					case BP:
						mat += 1;
						break;
					default:
						break;
				}
			}
		}
	}
	return mat;
}
void *menuRender(void* arg){
	resetSpot:
	// reset stuff
	unlimitedTime = false;
	opponentGone = false;
	gameStarted = false;
	guiHalt = false;
	guiHalted = false;
	canMove = true;
	displayBoard = true;
	whitesTurn = true;
	topUID.SetText("Anonymous");
	chatLists.fields.clear();
	chatLists.fields.push_back("Please be nice in chat!");
	chatLists.fields.push_back("Chat autoscrolls.");
	pieceSelected = false;
	selectedType = BR;
	wtime = 1234567;
	btime = 1234567;
	lastClockUpdate = 0;
	usleep(100);
	goToMainMenu = false;
	mainWin.RemoveAll();
	// end reset stuff
	
	GuiWindow bgWnd = GuiWindow(640, 480);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData signoutBtnOutline(signout_png);
	GuiImageData signoutBtnOutlineOver(signout_over_png);
	GuiImage signoutBtnImg(&signoutBtnOutline);
	GuiImage signoutBtnOverImg(&signoutBtnOutlineOver);
	GuiImage btnImg(&btnOutline);
	GuiImage btnOverImg(&btnOutlineOver);
	GuiImageData mainbg(mainbg_png);
	GuiTrigger trigA;
	GuiImage bgImg(640, 480, (GXColor){38, 36, 33, 255});

	GuiText signoutTxt("Sign Out", 10, (GXColor){0, 0, 0, 255});

	GuiText seekTxt("Seek in progress... Press the HOME button to cancel.", 20, (GXColor){255, 255, 255, 255});
	seekTxt.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
	seekTxt.SetPosition(0, 0);


	bgImg.SetImage(&mainbg);

	// sign out

	GuiButton signoutBtn(signoutBtnOutline.GetWidth()/2, signoutBtnOutline.GetHeight()/2);
	signoutBtnImg.SetScaleX(0.5f);
	signoutBtnImg.SetScaleY(0.5f);
	signoutBtnOverImg.SetScaleX(0.5f);
	signoutBtnOverImg.SetScaleY(0.5f);
	signoutBtn.SetImage(&signoutBtnImg);
	signoutBtn.SetImageOver(&signoutBtnOverImg);
	signoutBtn.SetLabel(&signoutTxt);
    signoutBtn.SetTrigger(&trigA);
	signoutBtn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	signoutBtn.SetPosition(-10, 10);
	signoutBtn.SetSoundOver(&btnSoundOver);
	signoutBtn.SetEffectGrow();

	// 10+0 Time Mode

	GuiImageData Ten0_btnOutline(_10_0_png);
	GuiImage Ten0_btnImg(&Ten0_btnOutline);
	GuiImageData Ten0_btnOutlineOver(_10_0_over_png);
	GuiImage Ten0_btnOverImg(&Ten0_btnOutlineOver);
	GuiButton Ten0_Btn(Ten0_btnOutline.GetWidth(), Ten0_btnOutline.GetHeight());

	Ten0_Btn.SetImage(&Ten0_btnImg);
	Ten0_Btn.SetImageOver(&Ten0_btnOverImg);
    Ten0_Btn.SetTrigger(&trigA);
	Ten0_Btn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	Ten0_Btn.SetPosition(8, 106);
	Ten0_Btn.SetSoundOver(&btnSoundOver);
	Ten0_Btn.SetAlpha(170);

	// 10+5 Time Mode

	GuiImageData Ten5_btnOutline(_10_5_png);
	GuiImage Ten5_btnImg(&Ten5_btnOutline);
	GuiImageData Ten5_btnOutlineOver(_10_5_over_png);
	GuiImage Ten5_btnOverImg(&Ten5_btnOutlineOver);

	GuiButton Ten5_Btn(Ten5_btnOutline.GetWidth(), Ten5_btnOutline.GetHeight());

	Ten5_Btn.SetImage(&Ten5_btnImg);
	Ten5_Btn.SetImageOver(&Ten5_btnOverImg);
    Ten5_Btn.SetTrigger(&trigA);
	Ten5_Btn.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	Ten5_Btn.SetPosition(0, 106);
	Ten5_Btn.SetSoundOver(&btnSoundOver);
	Ten5_Btn.SetAlpha(170);

	// 15+10 Time Mode

	GuiImageData Fifteen10_btnOutline(_15_10_png);
	GuiImage Fifteen10_btnImg(&Fifteen10_btnOutline);
	GuiImageData Fifteen10_btnOutlineOver(_15_10_over_png);
	GuiImage Fifteen10_btnOverImg(&Fifteen10_btnOutlineOver);

	GuiButton Fifteen10_Btn(Fifteen10_btnOutline.GetWidth(), Fifteen10_btnOutline.GetHeight());

	Fifteen10_Btn.SetImage(&Fifteen10_btnImg);
	Fifteen10_Btn.SetImageOver(&Fifteen10_btnOverImg);
    Fifteen10_Btn.SetTrigger(&trigA);
	Fifteen10_Btn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	Fifteen10_Btn.SetPosition(-8, 106);
	Fifteen10_Btn.SetSoundOver(&btnSoundOver);
	Fifteen10_Btn.SetAlpha(170);

	// 30+0 Time Mode

	GuiImageData Thirty0_btnOutline(_30_0_png);
	GuiImage Thirty0_btnImg(&Thirty0_btnOutline);
	GuiImageData Thirty0_btnOutlineOver(_30_0_over_png);
	GuiImage Thirty0_btnOverImg(&Thirty0_btnOutlineOver);

	GuiButton Thirty0_Btn(Thirty0_btnOutline.GetWidth(), Thirty0_btnOutline.GetHeight());

	Thirty0_Btn.SetImage(&Thirty0_btnImg);
	Thirty0_Btn.SetImageOver(&Thirty0_btnOverImg);
    Thirty0_Btn.SetTrigger(&trigA);
	Thirty0_Btn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	Thirty0_Btn.SetPosition(114, 245);
	Thirty0_Btn.SetSoundOver(&btnSoundOver);
	Thirty0_Btn.SetAlpha(170);

	// 30+20 Time Mode

	GuiImageData Thirty20_btnOutline(_30_20_png);
	GuiImage Thirty20_btnImg(&Thirty20_btnOutline);
	GuiImageData Thirty20_btnOutlineOver(_30_20_over_png);
	GuiImage Thirty20_btnOverImg(&Thirty20_btnOutlineOver);

	GuiButton Thirty20_Btn(Thirty20_btnOutline.GetWidth(), Thirty20_btnOutline.GetHeight());

	Thirty20_Btn.SetImage(&Thirty20_btnImg);
	Thirty20_Btn.SetImageOver(&Thirty20_btnOverImg);
    Thirty20_Btn.SetTrigger(&trigA);
	Thirty20_Btn.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	Thirty20_Btn.SetPosition(-114, 245);
	Thirty20_Btn.SetSoundOver(&btnSoundOver);
	Thirty20_Btn.SetAlpha(170);

	GuiText createChallengeTxt("Challenge a Friend", 15, (GXColor){0, 0, 0, 255});
	GuiImage createChallengeImg(&btnOutline);
	GuiImage createChallengeImgOver(&btnOutlineOver);

	GuiButton createChallenge(btnOutline.GetWidth(), btnOutline.GetHeight());

	createChallenge.SetImage(&createChallengeImg);
	createChallenge.SetImageOver(&createChallengeImgOver);
	createChallenge.SetLabel(&createChallengeTxt);
    createChallenge.SetTrigger(&trigA);
	createChallenge.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	createChallenge.SetPosition(-10, -10);
	createChallenge.SetSoundOver(&btnSoundOver);
	createChallenge.SetAlpha(170);
	
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	mainWin.SetState(STATE_DISABLED);
	bgWnd.SetState(STATE_DISABLED);

	bgWnd.Append(&bgImg);
	mainWin.Append(&signoutBtn);
	mainWin.Append(&Ten0_Btn);
	mainWin.Append(&Ten5_Btn);
	mainWin.Append(&Fifteen10_Btn);
	mainWin.Append(&Thirty0_Btn);
	mainWin.Append(&Thirty20_Btn);
	mainWin.Append(&createChallenge);
	mainWin.Append(&seekTxt);

	mainWin.SetState(STATE_DEFAULT);
	bgWnd.SetState(STATE_DEFAULT);

	char seekTxtAlpha = 0; // sort of like a byte, so limit is 255
	bool seekTxtAlternator = true;
	while (1){
		if(guiHalt)
		{
			guiHalted = true;
			while (guiHalt)
				usleep(100);
			guiHalted = false;
		} else {
			UpdatePads();
			u32 pressed = WPAD_ButtonsHeld(0);
			if ( pressed & WPAD_BUTTON_HOME ) 
				exitApp();
			if (seekTxtAlternator){
				seekTxtAlpha = seekTxtAlpha + 1;
			} else{
				seekTxtAlpha = seekTxtAlpha - 1;
			}
			if (seekTxtAlpha == 255 || seekTxtAlpha == 0){
				seekTxtAlternator = !seekTxtAlternator;
			}
			bgWnd.Draw();
			mainWin.Draw();
			for(int i=3; i >= 0; i--) // so that player 1's cursor appears on top!
			{
				if(userInput[i].wpad->ir.valid){
					Menu_DrawImg(userInput[i].wpad->ir.x-48, userInput[i].wpad->ir.y-48,
							96, 96, pointer[i]->GetImage(), userInput[i].wpad->ir.angle, 1, 1, 255);
				}
				DoRumble(i);
			}
			Menu_Render();
			seekTxt.SetVisible(inSeek);
			seekTxt.SetAlpha(seekTxtAlpha); // give the text a sort of "breathing" effect
			for(int i=0; i < 4; i++)
				mainWin.Update(&userInput[i]);
			if (Ten0_Btn.GetState() == STATE_CLICKED){
				if (!inSeek)
					createSeekExpeditor(10, 0);
				Ten0_Btn.SetState(STATE_DEFAULT);
			}
			if (Ten5_Btn.GetState() == STATE_CLICKED){
				if (!inSeek)
					createSeekExpeditor(10, 5);
				Ten5_Btn.SetState(STATE_DEFAULT);
			}
			if (Fifteen10_Btn.GetState() == STATE_CLICKED){
				if (!inSeek)
					createSeekExpeditor(15, 10);
				Fifteen10_Btn.SetState(STATE_DEFAULT);
			}
			if (Thirty0_Btn.GetState() == STATE_CLICKED){
				if (!inSeek)
					createSeekExpeditor(30, 0);
				Thirty0_Btn.SetState(STATE_DEFAULT);
			}
			if (Thirty20_Btn.GetState() == STATE_CLICKED){
				if (!inSeek)
					createSeekExpeditor(30, 20);
				Thirty20_Btn.SetState(STATE_DEFAULT);
			}
			if (createChallenge.GetState() == STATE_CLICKED){
				LWP_CreateThread(	&createChallengePromptHdl,	// thread handle 
									createChallengePromptThread,			// code 
									arg,		// arg pointer for thread 
									NULL,			// stack base 
									16*1024,		// stack size 
									80				// thread priority  
								);
				createChallenge.SetState(STATE_DEFAULT);
			}
			if (signoutBtn.GetState() == STATE_CLICKED){
				LWP_CreateThread(	&signOutHdl,	// thread handle 
									signOutPromptThread,			// code 
									arg,		// arg pointer for thread 
									NULL,			// stack base 
									16*1024,		// stack size 
									80				// thread priority  
								);
				signoutBtn.SetState(STATE_DEFAULT);
			}
			if (gameStarted){
				inSeek = false;
				gameRender(arg);
				goto resetSpot; // goto is a little bit old fashioned
				// but this will functionally reset so that the main menu functions properly
			}
		}
	}
}
void *gameRender(void* arg){
	LWP_CreateThread(	&gameNetworker_handle,	// thread handle 
						gameNetworker,			// code 
						arg,		// arg pointer for thread 
						NULL,			// stack base 
						16*1024,		// stack size 
						80				// thread priority  
					); // The game networker handles board events asynchronously and relays them to the gui for display.
	mainWin.RemoveAll(); // reset window
	GuiWindow bgWnd = GuiWindow(640, 480);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiImageData bgImgData(gamebg_png);
    GuiText chatBtnTxt("Send Chat Message", 14, (GXColor){0, 0, 0, 255});
	GuiImage chatBtnImg(&btnOutline);
	GuiImage chatBtnImgOver(&btnOutlineOver);
	GuiButton chatBtn(200, btnOutline.GetHeight());
	GuiTrigger trigA;
	GuiText topClock("NaN", 26, (GXColor){0, 0, 0, 255}); // it should never show NaN
	GuiText bottomClock("NaN", 26, (GXColor){0, 0, 0, 255}); // this should also never show NaN
	GuiText claimVictoryTxt("You may claim victory in ? seconds.", 12, (GXColor){255, 255, 255, 255});
	GuiImage bgImg(&bgImgData);

	GuiImageData abortbtnOutline(abortbtn_png);
	GuiImageData resignbtnOutline(resignbtn_png);
	GuiImageData drawbtnOutline(drawbtn_png);
	GuiImageData takebackbtnOutline(takebackbtn_png);
	GuiImageData menubtnOutline(menubtn_png);
	GuiImage abortBtnImg(&abortbtnOutline);
	GuiImage resignBtnImg(&resignbtnOutline);
	GuiImage drawBtnImg(&drawbtnOutline);
	GuiImage takebackBtnImg(&takebackbtnOutline);
	GuiImage menuBtnImg(&menubtnOutline);

	GuiButton abortBtn(40, abortbtnOutline.GetHeight());
	GuiButton menuBtn(40, menubtnOutline.GetHeight());
	GuiButton resignBtn(40, resignbtnOutline.GetHeight());
	GuiButton takebackBtn(40, takebackbtnOutline.GetHeight());
	GuiButton drawBtn(40, drawbtnOutline.GetHeight());

	abortBtn.SetImage(&abortBtnImg);
	abortBtn.SetImageOver(&abortBtnImg);
    abortBtn.SetTrigger(&trigA);
	abortBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	abortBtn.SetPosition(40, 362);
	abortBtn.SetSoundOver(&btnSoundOver);

	menuBtn.SetImage(&menuBtnImg);
	menuBtn.SetImageOver(&menuBtnImg);
    menuBtn.SetTrigger(&trigA);
	menuBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	menuBtn.SetPosition(0, 362);
	menuBtn.SetSoundOver(&btnSoundOver);

	resignBtn.SetImage(&resignBtnImg);
	resignBtn.SetImageOver(&resignBtnImg);
    resignBtn.SetTrigger(&trigA);
	resignBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	resignBtn.SetPosition(80, 362);
	resignBtn.SetSoundOver(&btnSoundOver);

	takebackBtn.SetImage(&takebackBtnImg);
	takebackBtn.SetImageOver(&takebackBtnImg);
    takebackBtn.SetTrigger(&trigA);
	takebackBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	takebackBtn.SetPosition(160, 362);
	takebackBtn.SetSoundOver(&btnSoundOver);

	drawBtn.SetImage(&drawBtnImg);
	drawBtn.SetImageOver(&drawBtnImg);
    drawBtn.SetTrigger(&trigA);
	drawBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	drawBtn.SetPosition(120, 362);
	drawBtn.SetSoundOver(&btnSoundOver);

	topClock.SetAlignment(ALIGN_RIGHT, ALIGN_TOP);
	topClock.SetPosition(-70, 16);
	bottomClock.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	bottomClock.SetPosition(-70, -14);

	topUID.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	topUID.SetPosition(215, 5);
	bottomUID.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	bottomUID.SetPosition(215, -26);

	topElo.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	topElo.SetPosition(215, 25);
	bottomElo.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	bottomElo.SetPosition(215, -6);

	claimVictoryTxt.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	claimVictoryTxt.SetPosition(0, -60);
	
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);
	chatBtn.SetImage(&chatBtnImg);
	chatBtn.SetImageOver(&chatBtnImgOver);
	chatBtn.SetLabel(&chatBtnTxt);
    chatBtn.SetTrigger(&trigA);
	chatBtn.SetAlignment(ALIGN_LEFT, ALIGN_TOP);
	chatBtn.SetPosition(0, 308);
	chatBtn.SetSoundOver(&btnSoundOver);
	mainWin.SetState(STATE_DISABLED);
	bgWnd.SetState(STATE_DISABLED);
	bgWnd.Append(&bgImg);
    mainWin.Append(&chatBtn);

	mainWin.Append(&drawBtn);
	mainWin.Append(&abortBtn);
	mainWin.Append(&takebackBtn);
	mainWin.Append(&resignBtn);
	mainWin.Append(&menuBtn);

	mainWin.Append(&topClock);
	mainWin.Append(&bottomClock);
	mainWin.Append(&topUID);
	mainWin.Append(&bottomUID);
	mainWin.Append(&topElo);
	mainWin.Append(&bottomElo);
	mainWin.Append(&claimVictoryTxt);
	mainWin.Append(chat);
	mainWin.SetState(STATE_DEFAULT);
	bgWnd.SetState(STATE_DEFAULT);
	while (1){
		if (goToMainMenu){
			return nullptr; // Ending gameRender should return to the main menu if called from the main menu.
		}
		if(guiHalt)
		{
			guiHalted = true;
			while (guiHalt)
				usleep(100);
			guiHalted = false;
		} else {
			UpdatePads();
			u32 pressed = WPAD_ButtonsHeld(0);
			if (!(pressed & WPAD_BUTTON_A)){
				if (pieceSelected){
					if (userInput[0].wpad->ir.valid){
						int x = userInput[0].wpad->ir.x;
						int y = userInput[0].wpad->ir.y;
						int x1 = ((640 - (SQUARE_SIZE * 8)) / 2) + 75; // real x coordinate of top left square
						int y1 = ((480 - (SQUARE_SIZE * 8)) / 2); // real y coordinate of top left square
						int x2 = ((x - x1) / SQUARE_SIZE); // x coordinate of square (0 - 7)
						int y2 = ((y - y1) / SQUARE_SIZE); // y coordinate of square (0 - 7)
						if (x > x1 && y > y1 && x < x1 + SQUARE_SIZE * 8 && y < y1 + SQUARE_SIZE * 8 && canMove) { // checks if the cursor is in board bounds and if player is allowed to make a move
							if ((selectedType <= 'F') != curColor && whitesTurn == curColor){ // nothing like b2b2 allowed (it messes up applyMove()), no stealing opponents pieces, make sure it's my turn before allowing a move
								std::string* txt = new std::string("");
								/*
								std::vector<std::pair<int, int>> moves = getLegalMoves(selectedPiece[0], 7 - selectedPiece[1]);
								for (int i = 0; i < moves.size(); i++){
									tmp = tmp + "(" + std::to_string(moves[i].first) + ", " + std::to_string(moves[i].second) + "), "; 
								}
								debugText = std::to_string(moves[0].second);
								*/
								if (!curColor){ // If the user is the black pieces, invert coordinates
									x2 = 7 - x2;
									y2 = 7 - y2;
								}
								txt->push_back(selectedPiece[0] + 'a');
								txt->push_back(8 - selectedPiece[1] + '0');
								txt->push_back(x2 + 'a');
								txt->push_back(8 - y2 + '0'); // construct the move in uci using the data
								debugText = *txt;
								if ((selectedType == WP || selectedType == BP) && ((selectedPiece[1] == 6 && y2 == 7) || (selectedPiece[1] == 1 && y2 == 0))){ // check if the move is a pawn promotion
									LWP_CreateThread(&moveHdl,	/* thread handle */
										&promotionPromptThread,			/* code */
										txt,		/* arg pointer for thread */
										NULL,			/* stack base */
										16*1024,		/* stack size */
										90				/* thread priority */ );
								} else { // if it's not a pawn promotion then follow standard move making procedure
									LWP_CreateThread(&moveHdl,	/* thread handle */
										&makeMoveThread,			/* code */
										txt,		/* arg pointer for thread */
										NULL,			/* stack base */
										16*1024,		/* stack size */
										90				/* thread priority */ );
								}
							}
						}
					}
				}
				pieceSelected = false;
			}
			bool checker = false;
			if (userInput[0].wpad->ir.valid && (pressed & WPAD_BUTTON_A)){
				int x = userInput[0].wpad->ir.x;
				int y = userInput[0].wpad->ir.y;
				int x1 = ((640 - (SQUARE_SIZE * 8)) / 2) + 75; // x coord of top left square
				int y1 = ((480 - (SQUARE_SIZE * 8)) / 2); // y coord of top left square
				if (x > x1 && y > y1 && x < x1 + SQUARE_SIZE * 8 && y < y1 + SQUARE_SIZE * 8 && canMove) { // check if cursor is in bounds of chess board and if user is allowed to move
					char p = curBoard[7 - ((y - y1) / SQUARE_SIZE)][((x - x1) / SQUARE_SIZE)]; // gets the selected piece based on cursor position
					if (!curColor){
						p = curBoard[((y - y1) / SQUARE_SIZE)][7 - (((x - x1) / SQUARE_SIZE))]; // if using black pieces, invert coordinates
					}
					if (p != '_' && !pieceSelected){ // check if the "piece" is not empty space and if another piece is not already selected
						// If so, set the currently selected piece to the one identified here
						pieceSelected = true;
						selectedPiece[0] = ((x - x1) / SQUARE_SIZE);
						selectedPiece[1] = ((y - y1) / SQUARE_SIZE);
						if (!curColor){
							selectedPiece[0] = 7 - ((x - x1) / SQUARE_SIZE);
							selectedPiece[1] = 7 - ((y - y1) / SQUARE_SIZE);
						}
						selectedType = (Pieces)p; 
					}
				}
			}
			int curtime = ticks_to_millisecs(gettime());
			if (opponentGone){
				if (whenCanClaimVictory >= curtime){
					std::string bufTxt = "You may claim victory in " + std::to_string((whenCanClaimVictory - curtime)/1000) + " seconds";
					claimVictoryTxt.SetText(bufTxt.c_str());
				} else {
					opponentGone = false;
					LWP_CreateThread(&opponentGoneHdl,	/* thread handle */
						&opponentGoneThread,			/* code */
						NULL,		/* arg pointer for thread */
						NULL,			/* stack base */
						16*1024,		/* stack size */
						90				/* thread priority */ );
				}
			}
			int selectedPieceIdx = 0;
			claimVictoryTxt.SetVisible(opponentGone);
			chatBtn.SetVisible(displayBoard);
			chat->SetVisible(displayBoard);
			menuBtn.SetVisible(displayBoard);
			abortBtn.SetVisible(displayBoard);
			resignBtn.SetVisible(displayBoard);
			drawBtn.SetVisible(displayBoard);
			takebackBtn.SetVisible(displayBoard);
			topClock.SetVisible(displayBoard);
			bottomClock.SetVisible(displayBoard);
			topElo.SetVisible(displayBoard);
			bottomElo.SetVisible(displayBoard);
			topUID.SetVisible(displayBoard);
			bottomUID.SetVisible(displayBoard); // displayBoard is usually false when keyboard is open, don't show other elements if keyboard is open
			bgWnd.Draw();
			if (displayBoard){
				int deltaTime = curtime - lastClockUpdate;
				if (curColor == whitesTurn) { // if white & whites turn, true, white & blacks turn, false, black and blacks turn, true, black and whites turn, false
					Menu_DrawRectangle(395 + 4*SQUARE_SIZE - 125, 5, 125, 50, (GXColor){255, 255, 255, 255}, true);
					Menu_DrawRectangle(395 + 4*SQUARE_SIZE - 125, 425, 125, 50, (GXColor){38, 36, 33, 255}, true); // these render the white boxes behind the clocks
					bottomClock.SetColor((GXColor){255, 255, 255, 255});
					topClock.SetColor((GXColor){0, 0, 0, 255});
				} else {
					Menu_DrawRectangle(395 + 4*SQUARE_SIZE - 125, 5, 125, 50, (GXColor){38, 36, 33, 255}, true);
					Menu_DrawRectangle(395 + 4*SQUARE_SIZE - 125, 425, 125, 50, (GXColor){255, 255, 255, 255}, true); // these render the white boxes behind the clocks
					bottomClock.SetColor((GXColor){0, 0, 0, 255});
					topClock.SetColor((GXColor){255, 255, 255, 255});
				} // this all just changes the color of the clock of whoever's turn it is
				if (unlimitedTime){
					bottomClock.SetText("Unlimited");
					topClock.SetText("Unlimited");
				}
				if (wtime - deltaTime >= 0 && btime - deltaTime >= 0 && !unlimitedTime){
					std::string wTimeStr = formatTime(wtime - (deltaTime * whitesTurn)); // if black's turn, whitesTurn = false = 0 making this just wtime
					std::string bTimeStr = formatTime(btime - (deltaTime * !whitesTurn)); // if white's turn, !whitesTurn = false = 0, making this just btime 
					if (curColor){ // (case where user is white pieces)
						bottomClock.SetText(wTimeStr.c_str());
						topClock.SetText(bTimeStr.c_str());
					} else { // (case where user is black pieces)
						bottomClock.SetText(bTimeStr.c_str());
						topClock.SetText(wTimeStr.c_str());
					}
				}
				for (int x = 0; x < 8 * SQUARE_SIZE; x+= SQUARE_SIZE){ // This entire block of code draws the board and the pieces on it
					for (int y = 0; y < 8 * SQUARE_SIZE; y+= SQUARE_SIZE){
						int ything = ((480 - (SQUARE_SIZE * 8)) / 2) + y;
						int xthing = ((640 - (SQUARE_SIZE * 8)) / 2) + 75 + x;
						if (!curColor){
							ything = ((480 - (SQUARE_SIZE * 8)) / 2) + ((7 * SQUARE_SIZE) - y);
							xthing = ((640 - (SQUARE_SIZE * 8)) / 2) + 75 + ((7 * SQUARE_SIZE) - x);
						}
						Menu_DrawRectangle(xthing, ything, SQUARE_SIZE, SQUARE_SIZE, boardColors[checker], true);
						char p = curBoard[7 - (y / SQUARE_SIZE)][x / SQUARE_SIZE];
						if (p != '_'){
							if (pieceSelected){
								if ((y / SQUARE_SIZE) == selectedPiece[1] && (x / SQUARE_SIZE) == selectedPiece[0] && userInput[0].wpad->ir.valid){
									selectedPieceIdx = p; // if we are looking at the selected piece, do not render it on the board but rather on the hand
									// also log its index for the future
									checker = !checker;
									continue;
								} else {
									Menu_DrawImg(xthing, ything, SQUARE_SIZE, SQUARE_SIZE, (pieces[p])->GetImage(), 0, 1, 1, 255);
								}
							}
							else{
								Menu_DrawImg(xthing, ything, SQUARE_SIZE, SQUARE_SIZE, (pieces[p])->GetImage(), 0, 1, 1, 255);
							}
						}
						checker = !checker;
					}
					checker = !checker;
				}
			}
			if (pieceSelected && userInput[0].wpad->ir.valid){
				Menu_DrawImg(userInput[0].wpad->ir.x - 24, userInput[0].wpad->ir.y - 32, SQUARE_SIZE, SQUARE_SIZE, (pieces[selectedPieceIdx])->GetImage(), 0, 1, 1, 255); // draw selected piece above rest
			}		
			mainWin.Draw();
			for(int i=3; i >= 0; i--) // so that player 1's cursor appears on top!
			{
				if(userInput[i].wpad->ir.valid){
					Menu_DrawImg(userInput[i].wpad->ir.x-48, userInput[i].wpad->ir.y-48,
							96, 96, pointer[i]->GetImage(), userInput[i].wpad->ir.angle, 1, 1, 255);
				}
				DoRumble(i);
			}
			Menu_Render();
			
			for(int i=0; i < 4; i++)
					mainWin.Update(&userInput[i]);
			if (chatBtn.GetState() == STATE_CLICKED){
				LWP_CreateThread(&kbHdl,	/* thread handle */
					&chatPromptThread,			/* code */
					NULL,		/* arg pointer for thread */
					NULL,			/* stack base */
					16*1024,		/* stack size */
					90				/* thread priority */ );
				chatBtn.SetState(STATE_DEFAULT);
			}
			if (abortBtn.GetState() == STATE_CLICKED){
				lichess::abortGame(gameId, tok);
				abortBtn.SetState(STATE_DEFAULT);
			}
			if (resignBtn.GetState() == STATE_CLICKED){
				lichess::resignGame(gameId, tok);
				resignBtn.SetState(STATE_DEFAULT);
			}
			if (drawBtn.GetState() == STATE_CLICKED){
				lichess::sendDrawOffer(gameId, tok);
				drawBtn.SetState(STATE_DEFAULT);
			}
			if (takebackBtn.GetState() == STATE_CLICKED){
				lichess::sendTakeback(gameId, tok);
				takebackBtn.SetState(STATE_DEFAULT);
			}
			if (menuBtn.GetState() == STATE_CLICKED){
				LWP_CreateThread(&menuPromptHdl,	/* thread handle */
					&menuPromptThread,			/* code */
					NULL,		/* arg pointer for thread */
					NULL,			/* stack base */
					16*1024,		/* stack size */
					90				/* thread priority */ );
				menuBtn.SetState(STATE_DEFAULT);
			}
		}
	}
}
std::vector<std::string> splitSpace(std::string str, char* delimiter = " ")
{
    std::vector<std::string> res;
    char *token = strtok(const_cast<char*>(str.c_str()), delimiter);
    while (token != nullptr)
    {
        res.push_back(std::string(token));
        token = strtok(nullptr, delimiter);
    }
  	return res;
}
void applyMove(char boardBuf[8][9], std::string move){ // takes a pointer to an array which represents a board and applies a move
		boardBuf[move[3] - 1 - '0'][move[2] - 'a'] = boardBuf[move[1] - 1 - '0'][move[0] - 'a'];
		boardBuf[move[1] - 1 - '0'][move[0] - 'a'] = '_';
		if (move == "e1g1" || move == "e8g8"){
			if (boardBuf[move[3] - 1 - '0'][move[2] - 'a'] == WK || boardBuf[move[3] - 1 - '0'][move[2] - 'a'] == BK){
				boardBuf[move[3] - 1 - '0'][5] = boardBuf[move[1] - 1 - '0'][7];
				boardBuf[move[1] - 1 - '0'][7] = '_';
			}
		}
		if (move == "e1c1" || move == "e8c8"){
			if (boardBuf[move[3] - 1 - '0'][move[2] - 'a'] == WK || boardBuf[move[3] - 1 - '0'][move[2] - 'a'] == BK){
				boardBuf[move[3] - 1 - '0'][3] = boardBuf[move[1] - 1 - '0'][0];
				boardBuf[move[1] - 1 - '0'][0] = '_';
			}
		}
		if (move.size() == 5){
			char p = boardBuf[move[3] - 1 - '0'][move[2] - 'a'];
			if (p == BP){ // only a pawn can cause 5 character long moves (promotion)
				switch (move[4]){
					case 'q':
						boardBuf[move[3] - 1 - '0'][move[2] - 'a'] = BQ;
						break;
					case 'r':
						boardBuf[move[3] - 1 - '0'][move[2] - 'a'] = BR;
						break;
					case 'n':
						boardBuf[move[3] - 1 - '0'][move[2] - 'a'] = BN;
						break;
					case 'b':
						boardBuf[move[3] - 1 - '0'][move[2] - 'a'] = BB;
						break;
					default:
						break;
				}
			} else{
				switch (move[4]){
					case 'q':
						boardBuf[move[3] - 1 - '0'][move[2] - 'a'] = WQ;
						break;
					case 'r':
						boardBuf[move[3] - 1 - '0'][move[2] - 'a'] = WR;
						break;
					case 'n':
						boardBuf[move[3] - 1 - '0'][move[2] - 'a'] = WN;
						break;
					case 'b':
						boardBuf[move[3] - 1 - '0'][move[2] - 'a'] = WB;
						break;
					default:
						break;
				}
			}
		}
}
void boardCallback(std::string data){
	rapidjson::Document doc; // Be very wary of draw and takeback offers
	if (data.find("\"type\":\"gameState\",") != std::string::npos || (data.find("draw\":") != std::string::npos && data.find("\"type\":\"chatLine\"") != std::string::npos)){
		if (data.find("sent") != std::string::npos){
			//WindowPrompt("Debug", std::to_string((int)(data.find("Draw") != std::string::npos)).c_str(), "a", "a");
			if (data.find("Draw") != std::string::npos || data.find("draw\":") != std::string::npos){
				chatLists.fields.push_back("lichess: Draw sent");
			} else {
				chatLists.fields.push_back("lichess: Takeback sent");
			}
			chat->TriggerUpdate();
			return;
		}
		if (data.find("cancelled") != std::string::npos){
			if (data.find("Draw") != std::string::npos || data.find("draw\":") != std::string::npos){
				chatLists.fields.push_back("lichess: Draw cancelled");
			} else {
				chatLists.fields.push_back("lichess: Takeback cancelled");
			}
			chat->TriggerUpdate();
			return;
		}
		if (data.find("accepted") != std::string::npos){
			//WindowPrompt("Debug", std::to_string((int)(data.find("Draw") != std::string::npos)).c_str(), "a", "a");
			if (data.find("Draw") != std::string::npos || data.find("draw\":") != std::string::npos){
				chatLists.fields.push_back("lichess: Draw accepted");
				notifySnd.Stop();
				notifySnd.Play();
				int res = WindowPrompt("Draw", "The game ended in a draw.", "Return to Loader", "Main Menu");
				switch (res){
					case 1:
						exitApp();
					case 0:
						goToMainMenu = true;
				}
			} else {
				chatLists.fields.push_back("lichess: Takeback accepted");
			}
			chat->TriggerUpdate();
			return;
		}
		if (data.find("declined") != std::string::npos){
			if (data.find("Draw") != std::string::npos || data.find("draw\":") != std::string::npos){
				chatLists.fields.push_back("lichess: Draw declined");
			} else {
				chatLists.fields.push_back("lichess: Takeback declined");
			}
			chat->TriggerUpdate();
			return;
		}
		// sometimes the takeback and gameState stream mix, seems to be lichess server's fault, surprisingly
		// so, I made this behemoth of a hack to fix it
		// testing this was weird; I hope my testing was comprehensive enough
	}
	if (data.find('\n') != std::string::npos && (data.find("offers draw") != std::string::npos || data.find("Draw offer") != std::string::npos || data.find("Takeback") != std::string::npos)){
		// for some reason, draw and takeback related events often end in a newline, causing immense headaches.
		asm(
			"nop"
		); // this delay is necessary because the entire thing crashes otherwise (it makes no sense)
		data = data.substr(0, data.size() - 1);
	}
	doc.Parse(data.c_str());
	std::string type = doc["type"].GetString();
	std::string uci;
	std::string status;
	if (type == "gameFull"){
		uci = doc["state"]["moves"].GetString();
		HaltGui();
		if (doc["white"].HasMember("name")){
			const char* wName = doc["white"]["name"].GetString();
			curColor = strcmp(wName, username.c_str()) == 0;
			if (!curColor){
				topUID.SetText(wName);
			} else {
				bottomUID.SetText(wName);
			}
		}
		if (doc["black"].HasMember("name")){
			const char* bName = doc["black"]["name"].GetString();
			curColor = strcmp(bName, username.c_str()) != 0;
			if (curColor){
				topUID.SetText(bName);
			} else{
				bottomUID.SetText(bName);
			}
		}
		if (doc["white"].HasMember("rating")){
			wElo = "(" + std::to_string(doc["white"]["rating"].GetInt()) + ")";
			if (!curColor){
				topElo.SetText(wElo.c_str());
			} else {
				bottomElo.SetText(wElo.c_str());
			}
		}
		if (doc["black"].HasMember("rating")){
			bElo = "(" + std::to_string(doc["black"]["rating"].GetInt()) + ")";
			if (curColor){
				topElo.SetText(bElo.c_str());
			} else {
				bottomElo.SetText(bElo.c_str());
			}
		}
		lastClockUpdate = ticks_to_millisecs(gettime());
		wtime = doc["state"]["wtime"].GetInt();
		btime = doc["state"]["btime"].GetInt();
		if (wtime == 2147483647){ // could also be btime
			unlimitedTime = true;
		}
		ResumeGui();
	} else if (type == "gameState"){
		uci = doc["moves"].GetString();
		status = doc["status"].GetString();
		HaltGui();
		lastClockUpdate = ticks_to_millisecs(gettime());
		wtime = doc["wtime"].GetInt();
		btime = doc["btime"].GetInt();
		ResumeGui();
	} else if (type == "chatLine"){
		std::string msg = doc["username"].GetString() + std::string(": ") + doc["text"].GetString();
		int a = msg.size() / 35;
		for (int i = 0; i <= a; i++){
			chatLists.fields.push_back(msg.substr(i * 35, 35));
		}
		chat->TriggerUpdate();
		return;
	} else if (type == "opponentGone") {
		HaltGui();
		bool gone = doc["gone"].GetBool();
		opponentGone = gone;
		if (gone){
			whenCanClaimVictory = ticks_to_millisecs(gettime()) + (doc["claimWinInSeconds"].GetInt() * 1000);
		}
		ResumeGui();
		return;
	} else {
		return;
	}
	char boardBuf[8][9];
	memcpy(boardBuf, startPos, sizeof (char) * 72); // create a buffer with the initial position
	std::vector<std::string> tokenized = splitSpace(uci);
	whitesTurn = (tokenized.size() % 2 == 0); // if the # of moves is even, then it is white's turn.
	for (size_t i = 0; i < tokenized.size(); i++){
		std::string move = tokenized[i];
		applyMove(boardBuf, move);
	} // apply all previous moves to this buffer
	HaltGui();
	int wMatBefore = materialEvaluator(curBoard, true);
	int bMatBefore = materialEvaluator(curBoard, false);
	memcpy(curBoard, boardBuf, sizeof(char) * 72);
	int wMat = materialEvaluator(curBoard, true);
	int bMat = materialEvaluator(curBoard, false);
	std::string bAdv = "+0";
	std::string wAdv = "+0";
	if (bMat > wMat){
		bAdv = "+" + std::to_string(bMat - wMat);
		wAdv = std::to_string(wMat - bMat);
	} else if (wMat > bMat){
		wAdv = "+" + std::to_string(wMat - bMat);
		bAdv = std::to_string(bMat - wMat);
	}
	if (curColor){
		topElo.SetText((bElo + ", " + bAdv).c_str());
		bottomElo.SetText((wElo + ", " + wAdv).c_str());
	} else {
		bottomElo.SetText((bElo + ", " + bAdv).c_str());
		topElo.SetText((wElo + ", " + wAdv).c_str());
	} // this all just displays the material disparities
	ResumeGui();
	// else if rollercoaster incoming!!!
	if (status == "resign"){
		wtime = 0; // stops counter
		notifySnd.Stop();
		notifySnd.Play();
		std::string boxmsg;
		std::string winnerOfMatch = doc["winner"].GetString();
		if (curColor == (winnerOfMatch == "white")){
			boxmsg = "You won by resignation.";
		} else {
			boxmsg = "You resigned.";
		}
		int res = WindowPrompt("Game Over", boxmsg.c_str(), "Return to Loader", "Main Menu");
		switch (res){
			case 1:
				exitApp();
			case 0:
				goToMainMenu = true;
		}
		return;
	} else if (status == "mate"){
		wtime = 0;
		notifySnd.Stop();
		notifySnd.Play();
		std::string boxmsg;
		std::string winnerOfMatch = doc["winner"].GetString();
		if (curColor == (winnerOfMatch == "white")){
			boxmsg = "You won by checkmate.";
		} else {
			boxmsg = "You were checkmated.";
		}
		int res = WindowPrompt("Game Over", boxmsg.c_str(), "Return to Loader", "Main Menu");
		switch (res){
			case 1:
				exitApp();
			case 0:
				goToMainMenu = true;
		}
		return;
	} else if (status == "draw"){
		wtime = 0;
		notifySnd.Stop();
		notifySnd.Play();
		int res = WindowPrompt("Draw", "The game ended in a draw.", "Return to Loader", "Main Menu");
		switch (res){
			case 1:
				exitApp();
			case 0:
				goToMainMenu = true;
		}
		return;
	} else if (status == "aborted" || status == "noStart"){
		wtime = 0;
		notifySnd.Stop();
		notifySnd.Play();
		int res = WindowPrompt("Game Over", "The game was aborted.", "Return to Loader", "Main Menu");
		switch (res){
			case 1:
				exitApp();
			case 0:
				goToMainMenu = true;
		}
		return;
	} else if (status == "timeout"){
		wtime = 0;
		notifySnd.Stop();
		notifySnd.Play();
		int res = WindowPrompt("Game Over", "Your opponent abandoned the game.", "Return to Loader", "Main Menu");
		switch (res){
			case 1:
				exitApp();
			case 0:
				goToMainMenu = true;
		}
		return;
	} else if (status == "outoftime"){
		wtime = 0;
		notifySnd.Stop();
		notifySnd.Play();
		std::string boxmsg = std::string(doc["winner"].GetString()) + " won on time.";
		int res = WindowPrompt("Game Over", boxmsg.c_str(), "Return to Loader", "Main Menu");
		switch (res){
			case 1:
				exitApp();
			case 0:
				goToMainMenu = true;
		}
		return;
	} else if (status == "stalemate"){
		wtime = 0;
		notifySnd.Stop();
		notifySnd.Play();
		int res = WindowPrompt("Game Over", "Draw by stalemate.", "Return to Loader", "Main Menu");
		switch (res){
			case 1:
				exitApp();
			case 0:
				goToMainMenu = true;
		}
		return;
	} else if (status == "cheat"){
		wtime = 0;
		notifySnd.Stop();
		notifySnd.Play();
		int res = WindowPrompt("Game Over", "Cheat Detected. (probably the opponent)", "Return to Loader", "Main Menu");
		switch (res){
			case 1:
				exitApp();
			case 0:
				goToMainMenu = true;
		}
		return;
	}
	if (type == "gameState" && whitesTurn == curColor){ // also checks if it's my turn or not
		if (wMat != wMatBefore || bMat != bMatBefore){ // check if material changes
			captureSnd.Stop();
			captureSnd.Play();
		} else {
			moveSnd.Stop();
			moveSnd.Play();
		}
	}
}

void eventCallback(std::string data){
	rapidjson::Document doc;
	doc.Parse(data.c_str());
	std::string type = doc["type"].GetString();
	if (type == "gameStart"){ // If a game starts (eg as a result of a search), go into it
		goToMainMenu = true; // just in case we're already in a game, clean reset
		usleep(100000);
		goToMainMenu = false;
		HaltGui();
		gameStarted = true;
		gameId = doc["game"]["gameId"].GetString();
		ResumeGui();
		notifySnd.Play();
	} 
	if (type == "challenge"){ // If a challenge is received, ask the user if they want to accept
		auto cDoc = doc["challenge"].GetObject();
		std::string ctrl;
		std::string typeTime = cDoc["timeControl"]["type"].GetString();
		if (typeTime == "clock") {
			ctrl = cDoc["timeControl"]["show"].GetString();
		} else if (typeTime == "unlimited"){
			ctrl = "unlimited time";
		} else if (typeTime == "correspondence"){
			return; // correspondence is not supported (yet) on Liichess
		}
		std::string challenger = cDoc["challenger"]["name"].GetString();
		if (challenger == username)
			return; // no outgoing challenges getting detected
		std::string challengerElo = "Unrated Challenge";
		if (cDoc["challenger"].HasMember("rating")){
			challengerElo = std::to_string(cDoc["challenger"]["rating"].GetInt());
		}
		std::string cId = cDoc["id"].GetString();
		std::string typeGame = "unrated";
		if (cDoc["rated"].GetBool()){
			typeGame = "rated";
		}
		std::string msg = challenger + " (" + challengerElo + ") has challenged you to a " + ctrl + ", " + typeGame + ", game. Accept?"; 
		std::string* challengeId = new std::string(cId);
		std::string* msgPtr = new std::string(msg.c_str());
		std::pair<std::string*, std::string*>* data = new std::pair<std::string*, std::string*>();
		data->first = challengeId;
		data->second = msgPtr;
		LWP_CreateThread(	&challengePromptHdl,	// thread handle 
							challengePromptThread,			// code 
							data,		// arg pointer for thread 
							NULL,			// stack base 
							16*1024,		// stack size 
							80				// thread priority  
						);
	}
}
void *eventStreamer(void* ptr) { // this just gives the even stream its own thread
	lichess::createEventStream(tok, &eventCallback);
	while (1){}
	return NULL;
}
void *gameNetworker(void* ptr) { // handles receiving of game related data
		std::string msgs = lichess::fetchChat(gameId, tok); // gets chats of the current game; should be empty, but you never know what can happen during network delay
		rapidjson::Document msgdoc;
		msgdoc.Parse(msgs.c_str());
		HaltGui();
		for (rapidjson::Value::ConstValueIterator itr = msgdoc.Begin(); itr != msgdoc.End(); itr++) {
			std::string msg = (*itr)["user"].GetString() + std::string(": ") + (*itr)["text"].GetString();
			int a = msg.size() / 35;
			for (int i = 0; i <= a; i++){
				chatLists.fields.push_back(msg.substr(i * 35, 35));
			}
		}
		chat->TriggerUpdate();
		ResumeGui();
		lichess::streamBoardState(gameId, tok, &boardCallback);
		while (!goToMainMenu){}
		return NULL;
}
int
WindowPrompt(const char *title, const char *msg, const char *btn1Label, const char *btn2Label)
{
	int choice = -1;

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (GXColor){0, 0, 0, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,40);
	GuiText msgTxt(msg, 22, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-20);
	msgTxt.SetWrap(true, 400);

	GuiText btn1Txt(btn1Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.GetWidth(), btnOutline.GetHeight());

	if(btn2Label)
	{
		btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
		btn1.SetPosition(20, -25);
	}
	else
	{
		btn1.SetAlignment(ALIGN_CENTRE, ALIGN_BOTTOM);
		btn1.SetPosition(0, -25);
	}

	btn1.SetLabel(&btn1Txt);
	btn1.SetImage(&btn1Img);
	btn1.SetImageOver(&btn1ImgOver);
	btn1.SetSoundOver(&btnSoundOver);
	btn1.SetTrigger(&trigA);
	btn1.SetState(STATE_SELECTED);
	btn1.SetEffectGrow();

	GuiText btn2Txt(btn2Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn2Img(&btnOutline);
	GuiImage btn2ImgOver(&btnOutlineOver);
	GuiButton btn2(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btn2.SetPosition(-20, -25);
	btn2.SetLabel(&btn2Txt);
	btn2.SetImage(&btn2Img);
	btn2.SetImageOver(&btn2ImgOver);
	btn2.SetSoundOver(&btnSoundOver);
	btn2.SetTrigger(&trigA);
	btn2.SetEffectGrow();

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&btn1);

	if(btn2Label)
		promptWindow.Append(&btn2);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	HaltGui();
	mainWin.SetState(STATE_DISABLED);
	mainWin.Append(&promptWindow);
	mainWin.ChangeFocus(&promptWindow);
	ResumeGui();

	while(choice == -1)
	{
		usleep(100);

		if(btn1.GetState() == STATE_CLICKED)
			choice = 1;
		else if(btn2.GetState() == STATE_CLICKED)
			choice = 0;
	}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(100);
	HaltGui();
	mainWin.Remove(&promptWindow);
	mainWin.SetState(STATE_DEFAULT);
	ResumeGui();
	return choice;
}
void createChallengePrompt() // this function is insane for something that should be so simple; it's almost as complex as the menu itself
{
	int timeChoice = 10;
	int incChoice = 0;
	bool ratedChoice = false;
	std::string cUser = "";

	GuiWindow promptWindow(448,288);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(keyboard_key_png);
	GuiImageData btnOutlineOver(keyboard_key_over_png);
	GuiImageData generalBtnOutline(button_png);
	GuiImageData generalBtnOutlineOver(button_over_png);
	GuiImageData boxOutline(keyboard_textbox_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt("Create a Challenge", 26, (GXColor){0, 0, 0, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0, 20);
	GuiText msgTxt("Time Mode:", 22, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0, -80);

	GuiText incrementTimeTxt("+", 22, (GXColor){0, 0, 0, 255});
	GuiImage incrementTimeImg(&btnOutline);
	GuiImage incrementTimeImgOver(&btnOutlineOver);
	GuiButton incrementTime(btnOutline.GetWidth(), btnOutline.GetHeight());

	incrementTime.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	incrementTime.SetPosition(60, -45);

	incrementTime.SetLabel(&incrementTimeTxt);
	incrementTime.SetImage(&incrementTimeImg);
	incrementTime.SetImageOver(&incrementTimeImgOver);
	incrementTime.SetSoundOver(&btnSoundOver);
	incrementTime.SetTrigger(&trigA);

	GuiText decrementTimeTxt("-", 22, (GXColor){0, 0, 0, 255});
	GuiImage decrementTimeImg(&btnOutline);
	GuiImage decrementTimeImgOver(&btnOutlineOver);
	GuiButton decrementTime(btnOutline.GetWidth(), btnOutline.GetHeight());

	decrementTime.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	decrementTime.SetPosition(-60, -45);

	decrementTime.SetLabel(&decrementTimeTxt);
	decrementTime.SetImage(&decrementTimeImg);
	decrementTime.SetImageOver(&decrementTimeImgOver);
	decrementTime.SetSoundOver(&btnSoundOver);
	decrementTime.SetTrigger(&trigA);

	GuiText incrementIncTxt("+", 22, (GXColor){0, 0, 0, 255});
	GuiImage incrementIncImg(&btnOutline);
	GuiImage incrementIncImgOver(&btnOutlineOver);
	GuiButton incrementInc(btnOutline.GetWidth(), btnOutline.GetHeight());

	incrementInc.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	incrementInc.SetPosition(60, 10);

	incrementInc.SetLabel(&incrementIncTxt);
	incrementInc.SetImage(&incrementIncImg);
	incrementInc.SetImageOver(&incrementIncImgOver);
	incrementInc.SetSoundOver(&btnSoundOver);
	incrementInc.SetTrigger(&trigA);

	GuiText decrementIncTxt("-", 22, (GXColor){0, 0, 0, 255});
	GuiImage decrementIncImg(&btnOutline);
	GuiImage decrementIncImgOver(&btnOutlineOver);
	GuiButton decrementInc(btnOutline.GetWidth(), btnOutline.GetHeight());

	decrementInc.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	decrementInc.SetPosition(-60, 10);

	decrementInc.SetLabel(&decrementIncTxt);
	decrementInc.SetImage(&decrementIncImg);
	decrementInc.SetImageOver(&decrementIncImgOver);
	decrementInc.SetSoundOver(&btnSoundOver);
	decrementInc.SetTrigger(&trigA);



	GuiText createTxt("Create", 22, (GXColor){0, 0, 0, 255});
	GuiImage createImg(&generalBtnOutline);
	GuiImage createImgOver(&generalBtnOutlineOver);
	GuiButton create(generalBtnOutline.GetWidth(), generalBtnOutline.GetHeight());

	create.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	create.SetPosition(20, -25);

	create.SetLabel(&createTxt);
	create.SetImage(&createImg);
	create.SetImageOver(&createImgOver);
	create.SetSoundOver(&btnSoundOver);
	create.SetTrigger(&trigA);

	GuiText cancelTxt("Cancel", 22, (GXColor){0, 0, 0, 255});
	GuiImage cancelImg(&generalBtnOutline);
	GuiImage cancelImgOver(&generalBtnOutlineOver);
	GuiButton cancel(generalBtnOutline.GetWidth(), generalBtnOutline.GetHeight());

	cancel.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancel.SetPosition(-20, -25);

	cancel.SetLabel(&cancelTxt);
	cancel.SetImage(&cancelImg);
	cancel.SetImageOver(&cancelImgOver);
	cancel.SetSoundOver(&btnSoundOver);
	cancel.SetTrigger(&trigA);


	GuiText timeTxt("10m", 26, (GXColor){0, 0, 0, 255});
	GuiText incTxt("0s", 26, (GXColor){0, 0, 0, 255});

	timeTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	incTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	
	timeTxt.SetPosition(0, -45);
	incTxt.SetPosition(0, 10);

	GuiText usernameTxt("Enter Username Here", 16, (GXColor){0, 0, 0, 255});
	GuiImage uBtnImg(&boxOutline);
	GuiImage uBtnOverImg(&boxOutline);
	GuiButton usernameBtn(boxOutline.GetWidth(), boxOutline.GetHeight());

	usernameBtn.SetImage(&uBtnImg);
	usernameBtn.SetImageOver(&uBtnOverImg);
	usernameBtn.SetLabel(&usernameTxt);
    usernameBtn.SetTrigger(&trigA);
	usernameBtn.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	usernameBtn.SetPosition(0, 50);
	usernameBtn.SetSoundOver(&btnSoundOver);

	GuiText ratedTxt("Rated", 16, (GXColor){0, 0, 0, 255});
	GuiImage ratedImg(&generalBtnOutline);
	GuiImage ratedOverImg(&generalBtnOutlineOver);
	GuiButton rated(generalBtnOutline.GetWidth() / 2, generalBtnOutline.GetHeight());

	ratedImg.SetScaleX(0.5f);
	ratedOverImg.SetScaleX(0.5f);
	rated.SetImage(&ratedImg);
	rated.SetImageOver(&ratedOverImg);
	rated.SetLabel(&ratedTxt);
    rated.SetTrigger(&trigA);
	rated.SetAlignment(ALIGN_LEFT, ALIGN_MIDDLE);
	rated.SetPosition(20, -18);
	rated.SetSoundOver(&btnSoundOver);

	GuiText unratedTxt("Unrated", 16, (GXColor){0, 0, 0, 255});
	GuiImage unratedImg(&generalBtnOutline);
	GuiImage unratedOverImg(&generalBtnOutlineOver);
	GuiButton unrated(generalBtnOutline.GetWidth() / 2, generalBtnOutline.GetHeight());

	unratedImg.SetScaleX(0.5f);
	unratedOverImg.SetScaleX(0.5f);
	unrated.SetImage(&unratedImg);
	unrated.SetImageOver(&unratedOverImg);
	unrated.SetLabel(&unratedTxt);
    unrated.SetTrigger(&trigA);
	unrated.SetAlignment(ALIGN_RIGHT, ALIGN_MIDDLE);
	unrated.SetPosition(-20, -18);
	unrated.SetSoundOver(&btnSoundOver);
	unrated.SetState(STATE_HELD);


	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&incrementTime);
	promptWindow.Append(&decrementTime);
	promptWindow.Append(&incrementInc);
	promptWindow.Append(&decrementInc);
	promptWindow.Append(&usernameBtn);
	promptWindow.Append(&unrated);
	promptWindow.Append(&rated);
	promptWindow.Append(&create);
	promptWindow.Append(&cancel);
	promptWindow.Append(&timeTxt);
	promptWindow.Append(&incTxt);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	HaltGui();
	mainWin.SetState(STATE_DISABLED);
	mainWin.Append(&promptWindow);
	mainWin.ChangeFocus(&promptWindow);
	ResumeGui();

	while(true)
	{
		usleep(100);
		if (incrementTime.GetState() == STATE_CLICKED){
			if (timeChoice >= 10 && timeChoice < 180){
				timeChoice++;
				std::string timeChoiceTxt = std::to_string(timeChoice) + "m";
				timeTxt.SetText(timeChoiceTxt.c_str());
			}
			incrementTime.SetState(STATE_DEFAULT);
		}
		if (decrementTime.GetState() == STATE_CLICKED){
			if (timeChoice > 10 && timeChoice <= 180){
				timeChoice--;
				std::string timeChoiceTxt = std::to_string(timeChoice) + "m";
				timeTxt.SetText(timeChoiceTxt.c_str());
			}
			decrementTime.SetState(STATE_DEFAULT);
		}
		if (incrementInc.GetState() == STATE_CLICKED){
			if (incChoice >= 0 && incChoice < 180){
				incChoice++;
				std::string incChoiceTxt = std::to_string(incChoice) + "s";
				incTxt.SetText(incChoiceTxt.c_str());
			}
			incrementInc.SetState(STATE_DEFAULT);
		}
		if (decrementInc.GetState() == STATE_CLICKED){
			if (incChoice > 0 && timeChoice <= 180){
				incChoice--;
				std::string incChoiceTxt = std::to_string(incChoice) + "s";
				incTxt.SetText(incChoiceTxt.c_str());
			}
			decrementInc.SetState(STATE_DEFAULT);
		}
		if (cancel.GetState() == STATE_CLICKED){
			cancel.SetState(STATE_DEFAULT);
			break;
		}
		if (create.GetState() == STATE_CLICKED){
			if (cUser == ""){
				WindowPrompt("Error", "You must enter a username.", "Ok", NULL);
			}
			if (lichess::createChallenge(timeChoice, incChoice, ratedChoice, cUser, tok)){
				WindowPrompt("Successfully Created", "Your challenge will expire in 20 seconds. Do not create any seeks or challenges until it expires or is accepted.", "Ok", NULL);
				break;
			} else {
				WindowPrompt("Error", "An error occured in creating the challenge.", "Ok", NULL);
			}
			create.SetState(STATE_DEFAULT);
		}
		if (rated.GetState() == STATE_CLICKED){
			ratedChoice = true;
			unrated.SetState(STATE_DEFAULT);
			rated.SetState(STATE_HELD);
		}
		if (unrated.GetState() == STATE_CLICKED){
			ratedChoice = false;
			rated.SetState(STATE_DEFAULT);
			unrated.SetState(STATE_HELD);
		}
		if (usernameBtn.GetState() == STATE_CLICKED){
			char usernameBuf[22] = "";
			OnScreenKeyboard(usernameBuf, 21);
			HaltGui();
			if (ratedChoice){
				unrated.SetState(STATE_DEFAULT);
				rated.SetState(STATE_HELD);
			} else {
				rated.SetState(STATE_DEFAULT);
				unrated.SetState(STATE_HELD);
			}
			mainWin.Remove(&promptWindow);
			mainWin.SetState(STATE_DISABLED);
			mainWin.Append(&promptWindow);
			mainWin.ChangeFocus(&promptWindow);
			ResumeGui();
			cUser = std::string(usernameBuf);
			usernameTxt.SetText(cUser.c_str());
			usernameBtn.SetState(STATE_DEFAULT);
		}
	}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(100);
	HaltGui();
	mainWin.Remove(&promptWindow);
	mainWin.SetState(STATE_DEFAULT);
	ResumeGui();
}
int
WindowPrompt4(const char *title, const char *msg, const char *btn1Label, const char *btn2Label, const char *btn3Label, const char *btn4Label)
{
	int choice = -1;

	GuiWindow promptWindow(448,328);
	promptWindow.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	promptWindow.SetPosition(0, -10);
	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiImageData dialogBox(dialogue_box_4_png);
	GuiImage dialogBoxImg(&dialogBox);

	GuiText titleTxt(title, 26, (GXColor){0, 0, 0, 255});
	titleTxt.SetAlignment(ALIGN_CENTRE, ALIGN_TOP);
	titleTxt.SetPosition(0,40);
	GuiText msgTxt(msg, 22, (GXColor){0, 0, 0, 255});
	msgTxt.SetAlignment(ALIGN_CENTRE, ALIGN_MIDDLE);
	msgTxt.SetPosition(0,-20);
	msgTxt.SetWrap(true, 400);

	GuiText btn1Txt(btn1Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn1Img(&btnOutline);
	GuiImage btn1ImgOver(&btnOutlineOver);
	GuiButton btn1(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn1.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	btn1.SetPosition(20, -85);
	btn1.SetLabel(&btn1Txt);
	btn1.SetImage(&btn1Img);
	btn1.SetImageOver(&btn1ImgOver);
	btn1.SetSoundOver(&btnSoundOver);
	btn1.SetTrigger(&trigA);
	btn1.SetState(STATE_SELECTED);
	btn1.SetEffectGrow();

	GuiText btn2Txt(btn2Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn2Img(&btnOutline);
	GuiImage btn2ImgOver(&btnOutlineOver);
	GuiButton btn2(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn2.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btn2.SetPosition(-20, -85);
	btn2.SetLabel(&btn2Txt);
	btn2.SetImage(&btn2Img);
	btn2.SetImageOver(&btn2ImgOver);
	btn2.SetSoundOver(&btnSoundOver);
	btn2.SetTrigger(&trigA);
	btn2.SetEffectGrow();

	GuiText btn3Txt(btn3Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn3Img(&btnOutline);
	GuiImage btn3ImgOver(&btnOutlineOver);
	GuiButton btn3(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn3.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	btn3.SetPosition(20, -25);
	btn3.SetLabel(&btn3Txt);
	btn3.SetImage(&btn3Img);
	btn3.SetImageOver(&btn3ImgOver);
	btn3.SetSoundOver(&btnSoundOver);
	btn3.SetTrigger(&trigA);
	btn3.SetEffectGrow();

	GuiText btn4Txt(btn4Label, 22, (GXColor){0, 0, 0, 255});
	GuiImage btn4Img(&btnOutline);
	GuiImage btn4ImgOver(&btnOutlineOver);
	GuiButton btn4(btnOutline.GetWidth(), btnOutline.GetHeight());
	btn4.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	btn4.SetPosition(-20, -25);
	btn4.SetLabel(&btn4Txt);
	btn4.SetImage(&btn4Img);
	btn4.SetImageOver(&btn4ImgOver);
	btn4.SetSoundOver(&btnSoundOver);
	btn4.SetTrigger(&trigA);
	btn4.SetEffectGrow();

	promptWindow.Append(&dialogBoxImg);
	promptWindow.Append(&titleTxt);
	promptWindow.Append(&msgTxt);
	promptWindow.Append(&btn1);
	promptWindow.Append(&btn2);
	promptWindow.Append(&btn3);
	promptWindow.Append(&btn4);

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_IN, 50);
	HaltGui();
	mainWin.SetState(STATE_DISABLED);
	mainWin.Append(&promptWindow);
	mainWin.ChangeFocus(&promptWindow);
	ResumeGui();

	while(choice == -1)
	{
		usleep(100);

		if(btn1.GetState() == STATE_CLICKED)
			choice = 1;
		else if(btn2.GetState() == STATE_CLICKED)
			choice = 2;
		else if(btn3.GetState() == STATE_CLICKED)
			choice = 3;
		else if(btn4.GetState() == STATE_CLICKED)
			choice = 4;
	}

	promptWindow.SetEffect(EFFECT_SLIDE_TOP | EFFECT_SLIDE_OUT, 50);
	while(promptWindow.GetEffect() > 0) usleep(100);
	HaltGui();
	mainWin.Remove(&promptWindow);
	mainWin.SetState(STATE_DEFAULT);
	ResumeGui();
	return choice;
}

/****************************************************************************
 * OnScreenKeyboard
 *
 * Opens an on-screen keyboard window, with the data entered being stored
 * into the specified variable.
 ***************************************************************************/
static void OnScreenKeyboard(char * var, u16 maxlen)
{
	int save = -1;

	GuiKeyboard keyboard(var, maxlen);

	GuiSound btnSoundOver(button_over_pcm, button_over_pcm_size, SOUND_PCM);
	GuiImageData btnOutline(button_png);
	GuiImageData btnOutlineOver(button_over_png);
	GuiTrigger trigA;
	trigA.SetSimpleTrigger(-1, WPAD_BUTTON_A | WPAD_CLASSIC_BUTTON_A, PAD_BUTTON_A);

	GuiText okBtnTxt("OK", 22, (GXColor){0, 0, 0, 255});
	GuiImage okBtnImg(&btnOutline);
	GuiImage okBtnImgOver(&btnOutlineOver);
	GuiButton okBtn(btnOutline.GetWidth(), btnOutline.GetHeight());

	okBtn.SetAlignment(ALIGN_LEFT, ALIGN_BOTTOM);
	okBtn.SetPosition(25, -25);

	okBtn.SetLabel(&okBtnTxt);
	okBtn.SetImage(&okBtnImg);
	okBtn.SetImageOver(&okBtnImgOver);
	okBtn.SetSoundOver(&btnSoundOver);
	okBtn.SetTrigger(&trigA);
	okBtn.SetEffectGrow();

	GuiText cancelBtnTxt("Cancel", 22, (GXColor){0, 0, 0, 255});
	GuiImage cancelBtnImg(&btnOutline);
	GuiImage cancelBtnImgOver(&btnOutlineOver);
	GuiButton cancelBtn(btnOutline.GetWidth(), btnOutline.GetHeight());
	cancelBtn.SetAlignment(ALIGN_RIGHT, ALIGN_BOTTOM);
	cancelBtn.SetPosition(-25, -25);
	cancelBtn.SetLabel(&cancelBtnTxt);
	cancelBtn.SetImage(&cancelBtnImg);
	cancelBtn.SetImageOver(&cancelBtnImgOver);
	cancelBtn.SetSoundOver(&btnSoundOver);
	cancelBtn.SetTrigger(&trigA);
	cancelBtn.SetEffectGrow();

	keyboard.Append(&okBtn);
	keyboard.Append(&cancelBtn);

	HaltGui();
	mainWin.SetState(STATE_DISABLED);
	mainWin.Append(&keyboard);
	mainWin.ChangeFocus(&keyboard);
	ResumeGui();

	while(save == -1)
	{
		usleep(100);

		if(okBtn.GetState() == STATE_CLICKED)
			save = 1;
		else if(cancelBtn.GetState() == STATE_CLICKED)
			save = 0;
	}

	if(save)
	{
		snprintf(var, maxlen, "%s", keyboard.kbtextstr);
	}

	HaltGui();
	mainWin.Remove(&keyboard);
	mainWin.SetState(STATE_DEFAULT);
	ResumeGui();
}
// For now, legal moves will be left for the server to decide.
/*std::vector<std::pair<int, int>> getLegalMoves(int x, int y){
	char p = curBoard[y][x];
	std::vector<std::pair<int, int>> res;
	if (p != '_'){
		switch (p){
			case WR:
				for (int i = x - 1; i >= 0; i--){
					if (curBoard[y][i] != '_'){
						if (curBoard[y][i] <= 'F')
							res.push_back(std::pair<int, int>(i, y));
						break;
					}
					res.push_back(std::pair<int, int>(i, y));
				}
				for (int i = x + 1; i <= 7; i++){
					if (curBoard[y][i] != '_'){
						if (curBoard[y][i] <= 'F')
							res.push_back(std::pair<int, int>(i, y));
						break;
					}
					res.push_back(std::pair<int, int>(i, y));
				}
				for (int i = y + 1; i <= 7; i++){
					if (curBoard[i][x] != '_'){
						if (curBoard[i][x] <= 'F')
							res.push_back(std::pair<int, int>(x, i));
						break;
					}
					res.push_back(std::pair<int, int>(x, i));
				}
				for (int i = y - 1; i >= 0; i--){
					if (curBoard[i][x] != '_'){
						if (curBoard[i][x] <= 'F')
							res.push_back(std::pair<int, int>(x, i));
						break;
					}
					res.push_back(std::pair<int, int>(x, i));
				}
				break;
			case BR:
				for (int i = x - 1; i >= 0; i--){
					if (curBoard[y][i] != '_'){
						if (curBoard[y][i] > 'F')
							res.push_back(std::pair<int, int>(i, y));
						break;
					}
					res.push_back(std::pair<int, int>(i, y));
				}
				for (int i = x + 1; i <= 7; i++){
					if (curBoard[y][i] != '_'){
						if (curBoard[y][i] > 'F')
							res.push_back(std::pair<int, int>(i, y));
						break;
					}
					res.push_back(std::pair<int, int>(i, y));
				}
				for (int i = y + 1; i <= 7; i++){
					if (curBoard[i][x] != '_'){
						if (curBoard[i][x] > 'F')
							res.push_back(std::pair<int, int>(x, i));
						break;
					}
					res.push_back(std::pair<int, int>(x, i));
				}
				for (int i = y - 1; i >= 0; i--){
					if (curBoard[i][x] != '_'){
						if (curBoard[i][x] > 'F')
							res.push_back(std::pair<int, int>(x, i));
						break;
					}
					res.push_back(std::pair<int, int>(x, i));
				}
				break;
			default:
				break;
		}
	}
	return res;
}*/
