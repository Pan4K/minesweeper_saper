

#include <stdlib.h>
#include <iostream>
#include <math.h>

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <list>
#include <iomanip>
#include <GL/glut.h>

#ifdef __APPLE__
#include <OpenGL/OpenGL.h>
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif



#include "Timer.h"
#include "Replay.h"
#include "common.h"
#include "Field.h"
#include "scores.h"


#define VERSION "0.5.2"

// TODO elapsed time isn't being redrawn while playing replay when there's a long pause between two 
//   events
// TODO prevent buffer overflows (strcpy and strcat)
// TODO free allocated memory (after using scores from loadScores and filterScores is finished)


using namespace std;




int windowWidth, windowHeight, originalWidth, originalHeight;
int squareSize;

int gameState; // -1 - initialized, 0 - playing, 1 - lost, 2 - won
char option_char;

unsigned short hitMineX,hitMineY;  // when game is lost
char playerName[21];
char highScoreDir[100];
bool isFlagging;
bool gamePaused;

bool playReplay;

int difficulty;   // 0-unspecified 1-beg 2-int 3-exp
    
Timer timer;

Replay replay;

Field field;

void endGameWon();








struct Mouse
{
	int x;		/*	the x coordinate of the mouse cursor	*/
	int y;		/*	the y coordinate of the mouse cursor	*/
	int lmb;	/*	is the left button pressed?		*/
	int mmb;	/*	is the middle button pressed?	*/
	int rmb;	/*	is the right button pressed?	*/

	/*
	 *	These two variables are a bit odd. Basically I have added these to help replicate
	 *	the way that most user interface systems work. When a button press occurs, if no
	 *	other button is held down then the co-ordinates of where that click occured are stored.
	 *	If other buttons are pressed when another button is pressed it will not update these
	 *	values.
	 *
	 *	This allows us to "Set the Focus" to a specific portion of the screen. For example,
	 *	in maya, clicking the Alt+LMB in a view allows you to move the mouse about and alter
	 *	just that view. Essentually that viewport takes control of the mouse, therefore it is
	 *	useful to know where the first click occured....
	 */
	int xpress; /*	stores the x-coord of when the first button press occurred	*/
	int ypress; /*	stores the y-coord of when the first button press occurred	*/
};


/*
 *	rename the structure from "struct Mouse" to just "Mouse"
 */
typedef struct Mouse Mouse;

/*
 *	Create a global mouse structure to hold the mouse information.
 */
Mouse TheMouse = {0,0,0,0,0};







typedef void (*ButtonCallback)();

/*
 *	This is a simple structure that holds a button.
 */
struct Button
{
	int   x;							/* top left x coord of the button */
	int   y;							/* top left y coord of the button */
	int   w;							/* the width of the button */
	int   h;							/* the height of the button */
	int	  state;						/* the state, 1 if pressed, 0 otherwise */
	int	  highlighted;					/* is the mouse cursor over the control? */
	char* label;						/* the text label of the button */
	ButtonCallback callbackFunction;	/* A pointer to a function to call if the button is pressed */
};

typedef struct Button Button;


void pauseGame();
void unpauseGame();

void PauseButtonPress()
{
  if (gameState==GAME_PLAYING and !playReplay) {
    if (!gamePaused) {
      pauseGame();
    }
    else {
      unpauseGame(); 
    }
  }
}

void RestartButtonPress()
{
  field.init();
}

void ExitButtonPress()
{
  exit(0);
}


Button PauseButton = {80,11, 80,25, 0,0, "Beertime", PauseButtonPress };
Button RestartButton = {170,11, 80,25, 0,0, "Restart", RestartButtonPress };
Button ExitButton = {260,11, 80,25, 0,0, "Exit", ExitButtonPress };





void Font(void *font,char *text,int x,int y)
{
	glRasterPos2i(x, y);

	while( *text != '\0' )
    {
      glutBitmapCharacter( font, *text );
      ++text;
    }
}


int ButtonClickTest(Button* b,int x,int y) 
{
	if( b) 
    {
      /*
       *	If clicked within button area, then return true
       */
	    if( x > b->x      && 
          x < b->x+b->w &&
          y > b->y      && 
          y < b->y+b->h ) {
				return 1;
      }
    }

	/*
	 *	otherwise false.
	 */
	return 0;
}

void ButtonRelease(Button *b,int x,int y)
{
	if(b) 
	{
		/*
		 *	If the mouse button was pressed within the button area
		 *	as well as being released on the button.....
		 */
		if( ButtonClickTest(b,TheMouse.xpress,TheMouse.ypress) && 
			ButtonClickTest(b,x,y) )
		{
			/*
			 *	Then if a callback function has been set, call it.
			 */
			if (b->callbackFunction) {
				b->callbackFunction();
			}
		}

		/*
		 *	Set state back to zero.
		 */
		b->state = 0;
	}
}

/*----------------------------------------------------------------------------------------
 *	\brief	This function draws the specified button.
 *	\param	b	-	a pointer to the button to check.
 *	\param	x	-	the x location of the mouse cursor.
 *	\param	y	-	the y location of the mouse cursor.
 */
void ButtonPress(Button *b,int x,int y)
{
	if(b)
	{
		/*
		 *	if the mouse click was within the buttons client area, 
		 *	set the state to true.
		 */
		if( ButtonClickTest(b,x,y) )
		{
			b->state = 1;
		}
	}
}


/*----------------------------------------------------------------------------------------
 *	\brief	This function draws the specified button.
 *	\param	b	-	a pointer to the button to check.
 *	\param	x	-	the x location of the mouse cursor.
 *	\param	y	-	the y location of the mouse cursor.
 */
void ButtonPassive(Button *b,int x,int y)
{
	if(b)
	{
		/*
		 *	if the mouse moved over the control
		 */
		if( ButtonClickTest(b,x,y) )
		{
			/*
			 *	If the cursor has just arrived over the control, set the highlighted flag
			 *	and force a redraw. The screen will not be redrawn again until the mouse
			 *	is no longer over this control
			 */
			if( b->highlighted == 0 ) {
				b->highlighted = 1;
				glutPostRedisplay();
			}
		}
		else
		/*
		 *	If the cursor is no longer over the control, then if the control
		 *	is highlighted (ie, the mouse has JUST moved off the control) then
		 *	we set the highlighting back to false, and force a redraw. 
		 */
		if( b->highlighted == 1 )
		{
			b->highlighted = 0;
			glutPostRedisplay();
		}
	}
}



void ButtonDraw(Button *b)
{
	int fontx;
	int fonty;

	if(b)
	{
		/*
		 *	We will indicate that the mouse cursor is over the button by changing its
		 *	colour.
		 */
		if (b->highlighted)
			glColor3f(0.7f,0.7f,0.8f);
		else
			glColor3f(0.6f,0.6f,0.6f);

		/*
		 *	draw background for the button.
		 */
		glBegin(GL_QUADS);
			glVertex2i( b->x     , b->y      );
			glVertex2i( b->x     , b->y+b->h );
			glVertex2i( b->x+b->w, b->y+b->h );
			glVertex2i( b->x+b->w, b->y      );
		glEnd();

		/*
		 *	Draw an outline around the button with width 3
		 */
		glLineWidth(3);

		/*
		 *	The colours for the outline are reversed when the button. 
		 */
		if (b->state) 
			glColor3f(0.4f,0.4f,0.4f);
		else 
			glColor3f(0.8f,0.8f,0.8f);

		glBegin(GL_LINE_STRIP);
			glVertex2i( b->x+b->w, b->y      );
			glVertex2i( b->x     , b->y      );
			glVertex2i( b->x     , b->y+b->h );
		glEnd();

		if (b->state) 
			glColor3f(0.8f,0.8f,0.8f);
		else 
			glColor3f(0.4f,0.4f,0.4f);

		glBegin(GL_LINE_STRIP);
			glVertex2i( b->x     , b->y+b->h );
			glVertex2i( b->x+b->w, b->y+b->h );
			glVertex2i( b->x+b->w, b->y      );
		glEnd();

		glLineWidth(1);


		/*
		 *	Calculate the x and y coords for the text string in order to center it.
		 */
		fontx = b->x + (b->w - glutBitmapLength(GLUT_BITMAP_HELVETICA_10, (unsigned char*)b->label)) / 2 ;
		fonty = b->y + (b->h+10)/2;

		/*
		 *	if the button is pressed, make it look as though the string has been pushed
		 *	down. It's just a visual thing to help with the overall look....
		 */
		if (b->state) {
			fontx+=2;
			fonty+=2;
		}

		/*
		 *	If the cursor is currently over the button we offset the text string and draw a shadow
		 */
		if(b->highlighted)
		{
			glColor3f(0,0,0);
			Font(GLUT_BITMAP_HELVETICA_10,b->label,fontx,fonty);
			fontx--;
			fonty--;
		}

		glColor3f(1,1,1);
		Font(GLUT_BITMAP_HELVETICA_10,b->label,fontx,fonty);
	}
}



void ordinalNumberSuffix(char *suffix, int n) {

    int rem100=n%100;

    if (rem100==11 or rem100==12 or rem100==13) {
        strcpy(suffix,"th");
        return;
    }

    switch(n%10) {
    case 1: strcpy(suffix,"st"); break;
    case 2: strcpy(suffix,"nd"); break;
    case 3: strcpy(suffix,"rd"); break;
    default: strcpy(suffix,"th"); break;
    }
    
}

void redisplay() {

    glutPostRedisplay();
    
}

bool directoryExists( const char* pzPath )
{
    if ( pzPath == NULL) return false;

    DIR *pDir;
    bool bExists = false;

    pDir = opendir (pzPath);

    if (pDir != NULL)
    {
        bExists = true;    
        (void) closedir (pDir);
    }

    return bExists;
}

// ---------------------- REPLAY ------------------//
void saveReplay(char *fname, Replay *r) {

    ofstream ofile;
    
    char fullpath[100];
    strcpy(fullpath,highScoreDir);
    strcat(fullpath,fname);
    cout << "Writing replay file " << fullpath <<"..."<<flush;

    ofile.open (fullpath);

    if (!ofile.is_open()) {
        cerr<<"Error opening file for writing replay."<<endl;
        return;
    }

 //   cout<< "Before r->writeToFile"<<endl;

    r->writeToFile(&ofile);
  //  cout<< "After r->writeToFile"<<endl;

    ofile.close();

    cout << "Done."<<endl;
   
}

int loadReplay(char *fname, Replay *r) {
    ifstream ifile;
/*
    char fullpath[100];
    strcpy(fullpath,highScoreDir);
    strcat(fullpath,fname);*/
 //   cout << "Reading replay file " << fullpath <<endl;

    ifile.open (fname);
    if (!ifile.is_open()) {
        cerr<<"Error opening replay file '"<<fname<<"'."<<endl;
        return 1;
    }

    r->readFromFile(&ifile);


    ifile.close();

    return 0;

}

// ----------------------- GRAPHICS -------------------- //

float mySin(float v) {
    return sin(v/ 180 * 3.141592654f);
}


float myCos(float v) {
    return cos(v/ 180 * 3.141592654f);
}

float myTan(float v) {
    return tan(v/ 180 * 3.141592654f);
}

void drawRect(float x, float y, float w, float h) {

    glBegin(GL_TRIANGLES);

    glVertex2f(x,y);
    glVertex2f(x+w,y);
    glVertex2f(x,y+h);

    glVertex2f(x,y+h);
    glVertex2f(x+w,y+h);
    glVertex2f(x+w,y);

    glEnd();
}

void drawDigitRect(int i, int x, int y, float zoom=1) {
    // draws a rectangle that's part of a digit

    float u;

    u=2*zoom; 

    
    switch(i) {
    case 0: drawRect(x,y,3*u,u); break;
    case 1: drawRect(x,y,u,3*u); break;
    case 2: drawRect(x+2*u,y,u,3*u); break;
    case 3: drawRect(x,y+2*u,3*u,u); break;
    case 4: drawRect(x,y+2*u,u,3*u); break;
    case 5: drawRect(x+2*u,y+2*u,u,3*u); break;
    case 6: drawRect(x,y+4*u,3*u,u); break;
    case 7: drawRect(x+u,y,u,3*u); break;
    case 8: drawRect(x+u,y+2*u,u,3*u); break;
    
    }


}


void drawDigit(int n, int x, int y, float zoom) {
    // draw a digit 10 px high, 6 px wide (if zoom==1)

    switch(n) {
    case 0:
        drawDigitRect(0,x,y,zoom);
        drawDigitRect(1,x,y,zoom);
        drawDigitRect(2,x,y,zoom);
        drawDigitRect(4,x,y,zoom);
        drawDigitRect(5,x,y,zoom);
        drawDigitRect(6,x,y,zoom);
        break;
    case 1:
        drawDigitRect(7,x,y,zoom);
        drawDigitRect(8,x,y,zoom);
        break;
    
    case 2:
        drawDigitRect(0,x,y,zoom);
        drawDigitRect(2,x,y,zoom);
        drawDigitRect(3,x,y,zoom);
        drawDigitRect(4,x,y,zoom);
        drawDigitRect(6,x,y,zoom);
        break;
    case 3:
        drawDigitRect(0,x,y,zoom);
        drawDigitRect(2,x,y,zoom);
        drawDigitRect(3,x,y,zoom);
        drawDigitRect(5,x,y,zoom);
        drawDigitRect(6,x,y,zoom);
        break;
    case 4:
        drawDigitRect(1,x,y,zoom);
        drawDigitRect(2,x,y,zoom);
        drawDigitRect(3,x,y,zoom);
        drawDigitRect(5,x,y,zoom);
        break;
    case 5:
        drawDigitRect(0,x,y,zoom);
        drawDigitRect(1,x,y,zoom);
        drawDigitRect(3,x,y,zoom);
        drawDigitRect(5,x,y,zoom);
        drawDigitRect(6,x,y,zoom);
        break;
    case 6:
        drawDigitRect(0,x,y,zoom);
        drawDigitRect(1,x,y,zoom);
        drawDigitRect(3,x,y,zoom);
        drawDigitRect(4,x,y,zoom);
        drawDigitRect(5,x,y,zoom);
        drawDigitRect(6,x,y,zoom);
        break;
    case 7:
        drawDigitRect(0,x,y,zoom);
        drawDigitRect(2,x,y,zoom);
        drawDigitRect(5,x,y,zoom);
        break;

    case 8:
        drawDigitRect(0,x,y,zoom);
        drawDigitRect(1,x,y,zoom);
        drawDigitRect(2,x,y,zoom);
        drawDigitRect(3,x,y,zoom);
        drawDigitRect(4,x,y,zoom);
        drawDigitRect(5,x,y,zoom);
        drawDigitRect(6,x,y,zoom);
        break;
    case 9:
        drawDigitRect(0,x,y,zoom);
        drawDigitRect(1,x,y,zoom);
        drawDigitRect(2,x,y,zoom);
        drawDigitRect(3,x,y,zoom);
        drawDigitRect(5,x,y,zoom);
        drawDigitRect(6,x,y,zoom);
        break;
    default:
        drawDigitRect(3,x,y,zoom);
        
    }

}


void drawScene() {


    glClearColor(.7, .7, .7, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


    glOrtho(0.0, windowWidth, windowHeight, 0.0, -1.0, 10.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();


    if (playReplay) {
        // draw cursor
        glColor3f(1,1,0);
        glBegin(GL_TRIANGLES);

        glVertex2f(replay.cursorX,replay.cursorY);
        glVertex2f(replay.cursorX,replay.cursorY+20);
        glVertex2f(replay.cursorX+11,replay.cursorY+17);
        glEnd();




    }

    // number of remaining mines
    glColor3f(1,0,0);
    int rem=field.calculateRemainingMines();


    if (rem>999) rem=999;

    int dxy=BORDER_WIDTH+DISPLAY_BORDER_WIDTH;

    for (int i=0;i<3;i++) {
        int digit=rem%10;
        rem/=10;
        drawDigit(digit,32-16*i+dxy,dxy,2);
    }

    glColor3f(0,0,0);
    drawRect(BORDER_WIDTH,
            BORDER_WIDTH,
            48+DISPLAY_BORDER_WIDTH,
            24+DISPLAY_BORDER_WIDTH);





    glDisable(GL_LIGHTING);
    glDisable(GL_DEPTH_TEST);
    ButtonDraw(&PauseButton);
    ButtonDraw(&RestartButton);
    ButtonDraw(&ExitButton);
    glEnable(GL_DEPTH_TEST);

    // elapsed time

    glColor3f(1,0,0);


    long etime=timer.calculateElapsedTime()/1000;


    if (etime>999) etime=999;

    int dx=originalWidth-BORDER_WIDTH-16;
    int dy=BORDER_WIDTH+DISPLAY_BORDER_WIDTH;

    for (int i=0;i<3;i++) {
        int digit=etime%10;
        etime/=10;
        drawDigit(digit,-16*i+dx,dy,2);
    }

    glColor3f(0,0,0);
    drawRect(dx-32-DISPLAY_BORDER_WIDTH,
            BORDER_WIDTH,
            16*3+DISPLAY_BORDER_WIDTH,
            24+DISPLAY_BORDER_WIDTH);

    if (gamePaused) {    // hide field when game is paused

        glBegin(GL_LINES);    
        glVertex2f(FIELD_X,FIELD_Y);
        glVertex2f(FIELD_X+field.width*squareSize,FIELD_Y);
        glEnd();

        glBegin(GL_LINES);    
        glVertex2f(FIELD_X,FIELD_Y+field.height*squareSize);
        glVertex2f(FIELD_X+field.width*squareSize,FIELD_Y+field.height*squareSize);
        glEnd();

        glBegin(GL_LINES);    
        glVertex2f(FIELD_X,FIELD_Y);
        glVertex2f(FIELD_X,FIELD_Y+field.height*squareSize);
        glEnd();

        glBegin(GL_LINES);    
        glVertex2f(FIELD_X+field.width*squareSize,FIELD_Y);
        glVertex2f(FIELD_X+field.width*squareSize,FIELD_Y+field.height*squareSize);
        glEnd();


        glColor3f(.5,.5,.5);

        glBegin(GL_TRIANGLES);
        glVertex2f(FIELD_X,FIELD_Y);
        glVertex2f(FIELD_X+field.width*squareSize,FIELD_Y);
        glVertex2f(FIELD_X+field.width*squareSize,FIELD_Y+field.height*squareSize);

        glVertex2f(FIELD_X,FIELD_Y);
        glVertex2f(FIELD_X,FIELD_Y+field.height*squareSize);
        glVertex2f(FIELD_X+field.width*squareSize,FIELD_Y+field.height*squareSize);


        
        glEnd();

    }
    else {

        // grid lines

        glColor3f(.2,.2,.2);

        

        for (int i=0;i<field.height+1;i++) {
            glBegin(GL_LINES);    
            glVertex2f(FIELD_X,FIELD_Y+i*squareSize);
            glVertex2f(FIELD_X+field.width*squareSize,FIELD_Y+i*squareSize);
            glEnd();

        }

        for (int i=0;i<field.width+1;i++) {
            glBegin(GL_LINES);    
            glVertex2f(FIELD_X+i*squareSize,FIELD_Y);
            glVertex2f(FIELD_X+i*squareSize,FIELD_Y+field.height*squareSize);
            glEnd();

        }

        // squares
        

        for (int x=0;x<field.width;x++) 
            for (int y=0;y<field.height;y++) {
                int x1=FIELD_X+x*squareSize;
                int x2=FIELD_X+(x+1)*squareSize;
                int y1=FIELD_Y+y*squareSize;
                int y2=FIELD_Y+(y+1)*squareSize;

                
                if (field.state[x][y]>=0 and field.state[x][y]<=8) {    // revealed square
                    
                    switch(field.state[x][y]) {
                    case 0: glColor3f(.5,.5,.5); break;
                    case 1: glColor3f(0,0,1); break;
                    case 2: glColor3f(0,1,0); break;
                    case 3: glColor3f(1,0,0); break;
                    case 4: glColor3f(0,0,.5); break;
                    case 5: glColor3f(.5,0,0); break;
                    case 6: glColor3f(0,1,1); break;
                    case 7: glColor3f(0,0,0); break;
                    case 8: glColor3f(.7,.7,.7); break;

                    default: glColor3f(0,0,0);
                    }


                    // number
                    float zoom=1.5*squareSize/25;

                    drawDigit(field.state[x][y],x1+.5*squareSize-3.0*zoom,
                                y1+.5*squareSize-5.0*zoom,zoom);
                    // background
                    glColor3f(.5,.5,.5);

                    glBegin(GL_TRIANGLES);
                    glVertex2f(x1,y1);
                    glVertex2f(x2,y1);
                    glVertex2f(x2,y2);

                    glVertex2f(x1,y1);
                    glVertex2f(x1,y2);
                    glVertex2f(x2,y2);
                            
                    glEnd();
                }
                else if (field.state[x][y]==9 
                         and (gameState==GAME_LOST or gameState==GAME_WON /* or true*/)
                         and field.isMine(x,y)
                        ) { // unflagged mine when game is over
                                        
                    glColor3f(0,0,0);
                    glBegin(GL_TRIANGLES);
                    glVertex2f(x1+.5*squareSize,y1+.1*squareSize);
                    glVertex2f(x1+.1*squareSize,y1+.5*squareSize);
                    glVertex2f(x1+.5*squareSize,y2-.1*squareSize);

                    glVertex2f(x1+.5*squareSize,y1+.1*squareSize);
                    glVertex2f(x2-.1*squareSize,y1+.5*squareSize);
                    glVertex2f(x1+.5*squareSize,y2-.1*squareSize);
                    
                    glEnd();

                    float gap=.25*squareSize;

                    glBegin(GL_TRIANGLES);
                    glVertex2f(x1+gap,y1+gap);
                    glVertex2f(x2-gap,y1+gap);
                    glVertex2f(x2-gap,y2-gap);

                    glVertex2f(x1+gap,y1+gap);
                    glVertex2f(x1+gap,y2-gap);
                    glVertex2f(x2-gap,y2-gap);
            
                    glEnd();
                }
                else if (field.state[x][y]==10) {  // flag

                    // cross out flag where there is no mine

                    if (gameState==GAME_LOST and !field.isMine(x,y)) {
                        float crossGap=.1*squareSize;
                        glColor3f(0,0,0);
                        glBegin(GL_LINES);
                        glVertex2f(x1+crossGap,y1+crossGap);
                        glVertex2f(x2-crossGap,y2-crossGap);

                        glVertex2f(x2-crossGap,y1+crossGap);
                        glVertex2f(x1+crossGap,y2-crossGap);
                        glEnd();
                    }
                    // flag

                    glColor3f(1,0,0);
                    glBegin(GL_TRIANGLES);
                    glVertex2f(x1+.1*squareSize,y1+.3*squareSize);
                    glVertex2f(x1+.6*squareSize,y1+.1*squareSize);
                    glVertex2f(x1+.6*squareSize,y1+.5*squareSize);
                    glColor3f(0,0,0);
                    glVertex2f(x1+.6*squareSize,y1+.6*squareSize);
                    glVertex2f(x1+.15*squareSize,y1+.9*squareSize);
                    glVertex2f(x1+.9*squareSize,y1+.9*squareSize);

                    glEnd();


                }



            }
    }
    // background for hit mine

    int hitMineDisplayX1=FIELD_X+hitMineX*squareSize;
    int hitMineDisplayX2=FIELD_X+(hitMineX+1)*squareSize;
    int hitMineDisplayY1=FIELD_Y+hitMineY*squareSize;
    int hitMineDisplayY2=FIELD_Y+(hitMineY+1)*squareSize;
    


    if (gameState==GAME_LOST) {
        glColor3f(1,0,0);

        glBegin(GL_TRIANGLES);
        glVertex2f(hitMineDisplayX1,hitMineDisplayY1);
        glVertex2f(hitMineDisplayX2,hitMineDisplayY1);
        glVertex2f(hitMineDisplayX2,hitMineDisplayY2);

        glVertex2f(hitMineDisplayX1,hitMineDisplayY1);
        glVertex2f(hitMineDisplayX1,hitMineDisplayY2);
        glVertex2f(hitMineDisplayX2,hitMineDisplayY2);
        glEnd();
    }

    glutSwapBuffers();

}


// ------------------------- GAME ---------------------- //

void pauseGame() {
  gamePaused=true;
  timer.pause(); 
  replay.pauseRecording();
  cout << "Game paused. Press P to continue. Elapsed time: "
       <<timer.calculateElapsedTime()<<" ms"<<endl;

}


void unpauseGame() {
    gamePaused=false;
    timer.unpause();
    replay.resumeRecording();
    cout << "Game unpaused."<<endl;// Elapsed time: "<<calculateElapsedTime()<<" ms"<<endl;
}

void endGameLost() {

    redisplay();

    cout << endl<< "YOU HIT A MINE. You played for "<<timer.calculateElapsedTime()
        <<" milliseconds." << endl;
    long timeTaken=timer.calculateElapsedTime();
    long ts=time(NULL);
    
    Score newScore;

    newScore.timeStamp=ts;
    strcpy(newScore.name,playerName);
    //char replayFile[100];
    newScore.width=field.width;
    newScore.height=field.height;
    newScore.mines=field.mineCount;

    newScore.time=timeTaken;
    newScore.val3BV=field.get3BV();
    newScore.flagging=isFlagging;


    newScore.effectiveClicks=field.effectiveClicks;
    newScore.ineffectiveClicks=field.ineffectiveClicks;

    //    cout << "IOE: " << newScore.getIOE()<<endl;

    newScore.squareSize=squareSize;
    newScore.gameWon=false;
    

    char fullpath[100];
    strcpy(fullpath,highScoreDir);
    strcat(fullpath,"scores.dat");


    newScore.replayNumber=0;
    appendScore(fullpath,newScore);

  //  cout << "width=" << field.width << endl;
    saveReplay("last.replay",&replay);

}

void endGameWon() {

    redisplay();

    gameState=GAME_WON;
    timer.stop();
    replay.stopRecording();
    
    if (!playReplay) {
        long timeTaken=timer.calculateElapsedTime();
        

        char rfname[100];
        long ts=time(NULL);

        long nr=1;

       
        Score newScore;

        newScore.timeStamp=ts;
        strcpy(newScore.name,playerName);
        //char replayFile[100];
        newScore.width=field.width;
        newScore.height=field.height;
        newScore.mines=field.mineCount;

        newScore.time=timeTaken;
        newScore.val3BV=field.get3BV();
        newScore.flagging=isFlagging;

        newScore.effectiveClicks=field.effectiveClicks;
        newScore.ineffectiveClicks=field.ineffectiveClicks;
        

        cout << endl<<"YOU WIN!"<<endl;
        cout << setw(8)<<left << "Time: "<<timeTaken<<" ms" << endl;
        cout << setw(8)<<left << "3BV: " << field.get3BV()<<endl;
        cout << setw(8)<<left << "3BV/s: " << setprecision(4)<< fixed<<newScore.get3BVs()<<endl;
        cout <<setw(8)<<left << "IOE: " << setprecision(4)<<fixed<< newScore.getIOE()<<endl;
        
        cout << "You played " << (isFlagging?"":"non-") << "flagging."<<endl;
        cout << endl;

        newScore.squareSize=squareSize;
        newScore.gameWon=true;
        

        char fullpath[100];
        strcpy(fullpath,highScoreDir);
        strcat(fullpath,"scores.dat");

        Score *scores;

     //   cout << "about to load scores"<<endl;

        int count=loadScores(fullpath,&scores);

     //   cout << "loaded scores"<<endl;

        evalScore(newScore,scores, count,difficulty);


    //    cout << "here1"<<endl;
        free(scores);
        

        // find the lowest unused replay file number

        cout << "Finding lowest unused replay number..."<<endl;

        while (true) {
    
            char tmp[100];
            strcpy(tmp,highScoreDir);
            sprintf(rfname,"%lu.replay",nr);

            strcat(tmp,rfname);
          //  cout << "Testing "<<tmp<<endl;
            ifstream testFile(tmp);
            if (!testFile)
                break;
            nr++;
        }

        newScore.replayNumber=nr;
        appendScore(fullpath,newScore);


        saveReplay(rfname,&replay);

   //     cout << "About to save replay."<<endl;

        saveReplay("last.replay",&replay);
    //    cout << "Done saving replay."<<endl;

    }


}

// -------------------------- GLUT ----------------------- //

void keyDown(unsigned char key, int x, int y) {

    switch (key) {
    case ' ':   
        if (!gamePaused and !playReplay) 
            field.init(); // restart game

        break;
    case 'p':   // pause
        if (gameState==GAME_PLAYING and !playReplay) {
            if (!gamePaused) {
              pauseGame();
            }
            else {
              unpauseGame(); 
            }
        }
        break;
    case 'r':
        replay.dump();
        break;
    case 'd':
        cout << sizeof(Score)<<endl;
        break;

    case 'q':
    case 27:    // escape
        exit(0);
    }


}


void mouseClick(int button, int mState, int x, int y) {
  TheMouse.x = x;
	TheMouse.y = y;

	/*
	 *	has the button been pressed or released?
	 */
  
	if (mState == GLUT_DOWN) {
    if ((TheMouse.lmb || TheMouse.mmb || TheMouse.rmb) ) {
      TheMouse.xpress = x;
      TheMouse.ypress = y;
    }

      /*
       *	Which button was pressed?
       */
      switch(button) 
        {
        case GLUT_LEFT_BUTTON:
          TheMouse.lmb = 1;
          ButtonPress(&PauseButton,x,y);
          ButtonPress(&RestartButton,x,y);
          ButtonPress(&ExitButton,x,y);
        case GLUT_MIDDLE_BUTTON:
          TheMouse.mmb = 1;
          break;
        case GLUT_RIGHT_BUTTON:
          TheMouse.rmb = 1;
          break;
        }
    }
	else 
    {
      /*
       *	Which button was released?
       */
      switch(button) 
        {
        case GLUT_LEFT_BUTTON:
          TheMouse.lmb = 0;
          ButtonRelease(&PauseButton,x,y);
          ButtonRelease(&RestartButton,x,y);
          ButtonRelease(&ExitButton,x,y);
          break;
        case GLUT_MIDDLE_BUTTON:
          TheMouse.mmb = 0;
          break;
        case GLUT_RIGHT_BUTTON:
          TheMouse.rmb = 0;
          break;
        }
    }


    if (!gamePaused) {
        if (gameState==GAME_INITIALIZED or gameState==GAME_PLAYING) {

            if (x>FIELD_X and x<FIELD_X+field.width*squareSize 
                and y>FIELD_Y and y<FIELD_Y+field.height*squareSize) {
                
                if (mState==GLUT_DOWN) {
                    
                  //  cout << "mouse button at [" << squareX << ", " << squareY << "], state "
                  // << state[squareX][squareY] << endl;
                
                    field.click(x,y,button);
                }
            }

            glutPostRedisplay();
        }
        else if (!(x>FIELD_X and x<FIELD_X+field.width*squareSize 
            and y>FIELD_Y and y<FIELD_Y+field.height*squareSize)) {
            field.init();
        //    placeMines();
        }
    }

}

 
void handleResize(int w, int h) {

    windowWidth=w;
    windowHeight=h;
    glViewport(0, 0, windowWidth, windowHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

}



void update(int value) {

   // cout << timer.calculateTimeSinceStart() << endl;
    glutPostRedisplay(); 
	
    glutTimerFunc(50, update, 0);
}


void updateR(int value) {

    
     
	
    int delay=replay.playStep();

   // cout << "delay=="<<delay<<endl;
    glutPostRedisplay();

    if (delay>=0) // if replay hasn't ended
        glutTimerFunc(delay, updateR, 0);
    else {
        cout << "End of replay."<<endl;
        glutTimerFunc(delay, update, 0);    // call the update function without replay functionality
    }

}



void mouseMove(int x, int y) {
    replay.recordEvent(x,y,-1);
}




void initGraph() {

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    windowWidth=FIELD_X+field.width*squareSize+BORDER_WIDTH;
    windowHeight=FIELD_Y+field.height*squareSize+BORDER_WIDTH;

    originalWidth=windowWidth;
    originalHeight=windowHeight;

    glutInitWindowSize(windowWidth, windowHeight);
        
    char title[100];

    strcpy(title,"Miny v");
    strcpy(title+6,VERSION);
    strcpy(title+6+strlen(VERSION),". Player: ");
    strcpy(title+16+strlen(VERSION),playerName);

    glutCreateWindow(title);
           
    glEnable(GL_DEPTH_TEST);     
                                          
    glutDisplayFunc(drawScene);
    glutKeyboardFunc(keyDown);
    glutReshapeFunc(handleResize);
    glutMouseFunc(mouseClick);
    glutPassiveMotionFunc(mouseMove);
    glutMotionFunc(mouseMove);
    
}

void initGraphR() {

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    windowWidth=FIELD_X+field.width*squareSize+BORDER_WIDTH;
    windowHeight=FIELD_Y+field.height*squareSize+BORDER_WIDTH;

    originalWidth=windowWidth;
    originalHeight=windowHeight;

    glutInitWindowSize(windowWidth, windowHeight);
        
    char title[100];

    strcpy(title,"Miny v");
    strcpy(title+6,VERSION);
    strcpy(title+6+strlen(VERSION),". Player: ");
    strcpy(title+16+strlen(VERSION),playerName);

    glutCreateWindow(title);
           
    glEnable(GL_DEPTH_TEST);     
                                          
    glutDisplayFunc(drawScene);
    glutKeyboardFunc(keyDown);
    glutReshapeFunc(handleResize);
 //   glutMouseFunc(mouseClick);
  //  glutPassiveMotionFunc(mouseMove);

}



int main(int argc, char** argv) {

    
    srand (time(NULL));

    glutInit(&argc, argv);

  //  Score *testarray=new Score[10000000000];


//    cout << "size="<<sizeof(int)<<endl;

/*
//debug

    difficulty=3;   // 0-unspecified 1-beg 2-int 3-exp
    
    Score s;
    s.flagging=false;

    s.time=10000;
    s.val3BV=90;

strcpy(highScoreDir,getenv("HOME"));
    strcat(highScoreDir,"/.miny/");


    char fullpath[100];
    strcpy(fullpath,highScoreDir);
    strcat(fullpath,"scores.dat");

    Score *scores;

 //   cout << "about to load scores"<<endl;

    int count=loadScores(fullpath,&scores);

 //   cout << "loaded scores"<<endl;

    evalScore(s,scores, count,difficulty);

    exit(0);

// end debug
*/
    field.height=16;
    field.width=16;
    field.mineCount=40;

    squareSize=0;
    gameState=GAME_INITIALIZED;
    gamePaused=false;

    char replayFileName[100];
    int playReplayPlace=-1;
    int listScoresType=0; // 0 - none, 1 - time, 2 - 3bv/s
    
    difficulty=0;   // 0-unspecified 1-beg 2-int 3-exp
    int listFlagging=0;  // 0-both, 1-flagging, 2-nf
    int listFinished=1; //  0-both, 1-finished, 2-unfinished
    int limit=MAX_HS;        // how many scores to display

    strcpy(highScoreDir,getenv("HOME"));
    strcat(highScoreDir,"/.miny/");

    playerName[0]='\0';


    strcpy(playerName,"");



    while ((option_char = getopt (argc, argv, "d:s:w:h:m:n:p:t3f:c:g:il:")) != -1)
        switch (option_char) {  
            case 'd': {
                difficulty=atoi(optarg);
                
                break;
            }
            case 's': 
                squareSize=atoi(optarg);
                break;
            case 'm': 
                difficulty=4;
                field.mineCount=atoi(optarg);
                break;
            case 'w': 
                difficulty=4;
                field.width=atoi(optarg);
                break;
            case 'h': 
                difficulty=4;
                field.height=atoi(optarg);
                break;
            case 'n':
                strncpy(playerName,optarg,20);
                
                break;
            case 'p':
                strcpy(replayFileName,highScoreDir);       
                strcat(replayFileName,optarg);
                playReplay=true;
                break;
            case 'r':
                playReplay=true;
                playReplayPlace=atoi(optarg)-1;
                break;
            case 'l':
                limit=atoi(optarg);
                break;
            case '3':
                listScoresType=2;
                break;
            case 't':
                listScoresType=1;
                break;
            case 'i':
                listScoresType=3;
                break;

            case 'f':
                listFlagging=optarg[0]-'0';
            //    cout << "listFlagging="<<listFlagging<<endl;
                break;
            case 'g':
                listFinished=optarg[0]-'0';
             //   cout << "listFinished="<<listFinished<<endl;
                break;
            case 'c':   
                strcpy(replayFileName,optarg);
                if (loadReplay(replayFileName,&replay)) {
                    exit(1);
                }
                cout << field.get3BV() << endl;
                exit(0);
                break;
            case '?':
                //cerr << "Unknown option: -" << optopt << endl;
                exit(1);

        }


    cout<<"Pan4k_"<<VERSION<<endl;
    cout << "See README for info and help."<<endl;

    // high score directory
    //cout << "hsdir: " << highScoreDir<<endl;

    if (!directoryExists(highScoreDir)) {
        if (system("mkdir ~/.miny")) {
            cerr << "Error creating config directory. Exiting."<<endl;
            exit(1);
        }
    /*    else {
            cout << "Config directory created."<<endl;
        }*/

    }
 /*   else {
        cout<<"Config directory exists."<<endl;
    }*/

    if (playReplay) {

        cout << replayFileName<<endl;
        
        if (loadReplay(replayFileName,&replay)) {
            exit(1);
        }
        cout<<"Replay loaded."<<endl;
        field.init();
        cout << "Playing replay..." << endl;
        initGraphR();
        

        glutTimerFunc(1, updateR, 0);
    }
    else {
        if (listScoresType!=0) { // list scores

            // TODO 'other' setups may produce too high 3BV/s etc and break layout

            char fullpath[100];
            strcpy(fullpath,highScoreDir);
            strcat(fullpath,"scores.dat");


            Score *scores;
            int count=loadScores(fullpath,&scores);

       //     cout << "Loaded " << count << " scores."<<endl;
            

            if (count==0) {
                cout<<"No high scores yet."<<endl;
            }
            else {
                
                cout << endl << "Displaying scores."<<endl;
                cout << setw(16)<<left<<"Sorted by: ";

                int (*compareFunc)(const void *,const void *)=NULL;
                switch(listScoresType) {
                case 1:    //time
                    cout << "time" << endl;
                    compareFunc=compareByTime; break;
                case 2:
                    cout << "3BV/s" << endl;
                    compareFunc=compareBy3BVs; break;
                case 3:
                    cout << "IOE" << endl;
                    compareFunc=compareByIOE; break;
                }
    
                cout << setw(16)<<left<<"Flagging: ";
                switch (listFlagging) {
                case 0: cout << "all"<<endl; break;
                case 1: cout << "flagging only"<<endl; break;
                case 2: cout << "non-flagging only"<<endl; break;
                }

                cout << setw(16)<<left<<"Finished: ";
                switch (listFinished) {
                case 0: cout << "all"<<endl; break;
                case 1: cout << "finished only"<<endl; break;
                case 2: cout << "unfinished only"<<endl; break;
                }

                cout << setw(16)<<left<<"Difficulty: ";
                switch (difficulty) {
                case 0: cout << "beginner, intermediate, expert"<<endl; break;
                case 1: cout << "beginner only"<<endl; break;
                case 2: cout << "intermediate only"<<endl; break;
                case 3: cout << "expert only"<<endl; break;
                }

                cout << setw(16)<<left<<"Square size: ";
                if (squareSize!=0)
                    cout <<squareSize<<endl;
                else
                    cout << "all"<<endl;

                cout << setw(16)<<left<<"Player name: ";
                if (!strcmp(playerName,""))
                    cout<<"all"<<endl;
                else
                    cout<<playerName<<endl;

                cout << setw(16)<<left<<"Count: ";
                if (limit!=0)
                    cout <<limit<<endl;
                else
                    cout << "all"<<endl;


                qsort(scores,count,sizeof(Score),compareFunc);


                Score *filteredScores;


                count=filterScores(scores, count, &filteredScores,listFlagging, listFinished,
                    difficulty, squareSize,playerName);

                displayScores(filteredScores,count,limit);

                cout<<endl;
           //     cout << "here2"<<endl;
                free(scores);

            }


        }
        else {


            if (difficulty==0) 
                difficulty=2;


            switch(difficulty) {
                case 1:
                    field.height=9;
                    field.width=9;
                    field.mineCount=10;
                    break;
                case 2:
                    field.height=16;
                    field.width=16;
                    field.mineCount=40;
                    break;
                case 3:
                    field.height=16;
                    field.width=30;
                    field.mineCount=99;
                    break;
            }

            if (squareSize==0)
                squareSize=25;

            if (squareSize<3) 
                squareSize=3;
            else if (squareSize>100) 
                squareSize=100;
            
            field.checkValues();


            char *validChars="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ01234567890_";
            char *c=playerName;

            while(*c) {
                if (!strchr(validChars,*c)) {
                    cout << "You entered an invalid name. Name can be max. 20 characters long and can only "
                    <<endl<<"contain the characters a-z, A-Z, 0-9 and underscore (_)."<<endl;
                    exit(1);    
                }
                c++;
            }


            if (strlen(playerName)>20) {
                cout << "You entered an invalid name. Name can be max. 20 characters long and can only "
                    <<endl<<"contain the characters a-z, A-Z, 0-9 and underscore (_)."<<endl;
                exit(1);
            }


            if (!strcmp(playerName,"")) {   
              

                cout << "No name entered with the -n option. Using 'unnamed'." << endl;
                strcpy(playerName,"unnamed");
            }

            cout << "Your name: "<<playerName<<endl;

            initGraph();
            field.init();

            glutTimerFunc(50, update, 0);
        }
    }

    glutMainLoop();    
    return 0;

}
