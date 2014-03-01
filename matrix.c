

#include <curses.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "i2c-dev.h"
#include <fcntl.h>
#include "8x8font.h"
#include <string.h>
#include <termios.h>
#include <sys/time.h>
#include <time.h>


#define MATRIX1 0x70
#define MATRIX2 0x71
#define MATRIX3 0x72
#define H_DELAY 100
#define V_DELAY 100

#define VERTICAL 0
void selectDevice(int file, int addr);
int mymillis();


__u16 block[I2C_SMBUS_BLOCK_MAX];
//global variables used for matrix
int res, i2cbus, daddress, address, size, file;

//Reverse the bits
unsigned  char  reverseBits(unsigned  char num)
{
    unsigned  char count = sizeof(num) * 8 - 1;
    unsigned  char reverse_num = num;
    num >>= 1;
    while(num)
    {
       reverse_num <<= 1;
       reverse_num |= num & 1;
       num >>= 1;
       count--;
    }
    reverse_num <<= count;
    return reverse_num;
}



/* Print n as a binary number */
void printbitssimple(char n) {
        unsigned char i;
        i = 1<<(sizeof(n) * 8 - 1);

        while (i > 0) {
                if (n & i)
                        printf("#");
                else
                        printf(".");
                i >>= 1;
                }
        printf("\n");
        }


int displayImage(__u16 bmp[], int res, int daddress, int file)
{
        int i;
        for(i=0; i<8; i++)
        {
             block[i] = (bmp[i]&0xfe) >>1 |
             (bmp[i]&0x01) << 7;
        }
        res = i2c_smbus_write_i2c_block_data(file, daddress, 16,
                (__u8 *)block);
}

void  INThandler(int sig)
{
       // Closing file and turning off Matrix

        unsigned short int clear[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
        int i;
        for (i = MATRIX1 ; i<0x74;i++){
        selectDevice(file,i);
                displayImage(clear,res, daddress, file);
}
        printf("Closing file and turning off the LED Matrix\n");
        daddress = 0x20;
        for(daddress = 0xef; daddress >= 0xe0; daddress--) {
                res = i2c_smbus_write_byte(file, daddress);
        }

        endwin();
        signal(sig, SIG_IGN);

        close(file);
        exit(0);
}


int main(int argc, char *argv[])
{

        char *end;
        int count,cont;
        char filename[20];
        unsigned char letter;
        int i,t,y;
        unsigned int delay=100000;
        i2cbus   = 1;
        address  = MATRIX1;
        daddress = 0;
        char text[strlen(argv[1])+5];
        char ch;
        int verticalTimer  = mymillis();
        int horizontalTimer  = mymillis();
        int h_delay = H_DELAY;
        int v_delay = V_DELAY;
        unsigned short int   displayBuffer[8];
        unsigned short int   h_bitShifted[8];
        unsigned short int   v_bitShifted[8];

        char direction = 'l';
        int h;


        signal(SIGINT, INThandler);


     //Startup the matrix
        size = I2C_SMBUS_BYTE;
        sprintf(filename, "/dev/i2c-%d", i2cbus);
        file = open(filename, O_RDWR);
        if (file<0) {
                if (errno == ENOENT) {
                        fprintf(stderr, "Error: Could not open file "
                                "/dev/i2c-%d: %s\n", i2cbus, strerror(ENOENT));
                        }
                 else {
                        fprintf(stderr, "Error: Could not open file "

                                "`%s': %s\n", filename, strerror(errno));
                        if (errno == EACCES)
                                fprintf(stderr, "Run as root?\n");
                }
                exit(1);
        }

        if (ioctl(file, I2C_SLAVE, address) < 0) {
                fprintf(stderr,
                        "Error: Could not set address to 0x%02x: %s\n",
                        address, strerror(errno));
                return -errno;
        }


        res = i2c_smbus_write_byte(file, daddress);
        if (res < 0) {
                fprintf(stderr, "Warning - write failed, filename=%s, daddress=%d\n",
                        filename, daddress);
        }

        //turn all maxtices on
        for (i = MATRIX1 ; i<0x73;i++){
                selectDevice(file,i);

                daddress = 0x21; // Start oscillator (page 10)
                printf("writing: 0x%02x\n", daddress);
                res = i2c_smbus_write_byte(file, daddress);

                daddress = 0x81; // Display on, blinking off (page 11)
                printf("writing: 0x%02x\n", daddress);
                res = i2c_smbus_write_byte(file, daddress);

                daddress = 0xef; // Full brightness (page 15)
                printf("Full brightness writing: 0x%02x\n", daddress);
                res = i2c_smbus_write_byte(file, daddress);

                daddress = 0x00; // Start writing to address 0 (page 13)
                printf("Start writing to address 0 writing: 0x%02x\n", daddress);
                res = i2c_smbus_write_byte(file, daddress);

        }

        //Setup the text  argument that was passed to main. remove null and add some extra spaces.
        //add two blank lines at the beginning
        text[0] = 32;
        text[1] = 32;
        //copy the string from argv
        for(i = 0; i < (strlen(argv[1])) ; i++){
                text[i+2] = argv[1][i];
        }

        //add some spaces to the end
        for(i = 0; i < 5 ; i++){
                text[strlen(argv[1])+2+i] = 32;
        }

        //put all the characters of the scrolling text in a contiguous block
        int length = (strlen(text))-1;
        int Vposition,c,character, l;
        unsigned short int   contiguousText[11][length*8];
        unsigned short int   display[8];
        for(i = 0; i < length ; i++){
                character = (text[i]-31);
                 for(Vposition = 0; Vposition < 8 ; Vposition++){
                        contiguousText[Vposition][i] = FONT8x8[character][Vposition];
                }
                contiguousText[8][i]=0;
                contiguousText[9][i]=0;
                contiguousText[10][i]=0;

        }
        //use ncurses to puth terminal in no delay mode so keystrokes can be inputted  and there is no need to hit return after each keystroke
        initscr();
        noecho();
        nodelay(stdscr, TRUE);


        letter = 0;

     while (1){
                //get keystroke input
                ch = getch();
                if (ch == 'u') v_delay+=5;
                if (ch == 'd') v_delay-=5;
                if (ch == 'l') direction = 'l';
                if (ch == 'r') direction = 'r';
                if (ch == '-') h_delay-=5;
                if (ch == '+') h_delay+=5;

                //Vertical scrolling
                if (VERTICAL){
                        if (mymillis() - verticalTimer > v_delay){      //the delay controls the speed of the scroll
                                for(i = 0; i < length ; i++){
                                        contiguousText[10][i] = contiguousText[0][i];
                                                for(h = 0; h<10; h++){
                                                        contiguousText[h][i] = contiguousText[h+1][i] ;
                                                }
                                }
                                verticalTimer = mymillis();
                        }
                }

                if(mymillis() - horizontalTimer > h_delay){             //the delay controls the speed of the scroll

                        //scroll left or scroll right
                        if (direction == 'l'){
                                if (y>6){
                                        y=0;
                                        if (letter>(length-3))
                                                letter=0;
                                        else
                                                letter++;
                                }
                        else
                                y++;

                        selectDevice(file,MATRIX1);
                        for(i=0; i<8; i++){
                                h_bitShifted[i] = (contiguousText[i][letter]) << y | (contiguousText[i][letter+1]) >> (8-y);
                                h_bitShifted[i]  = reverseBits(h_bitShifted[i]);
                        }
                        displayImage(h_bitShifted,res, daddress, file);

                        if (letter>0){
                                selectDevice(file,MATRIX2);
                                for(i=0; i<8; i++){
                                        h_bitShifted[i] = (contiguousText[i][letter-1]) << y | (contiguousText[i][letter]) >> (8-y);
                                        h_bitShifted[i] = reverseBits(h_bitShifted[i]);
                                }
                                displayImage(h_bitShifted,res, daddress, file);
                        }

                        if (letter>1){
                                selectDevice(file,MATRIX3);
                                for(i=0; i<8; i++){
                                        h_bitShifted[i] = (contiguousText[i][letter-2]) << y | (contiguousText[i][letter-1]) >> (8-y);
                                        h_bitShifted[i] = reverseBits(h_bitShifted[i]);
                                }
                                displayImage(h_bitShifted,res, daddress, file);
                        }
                }
                else{

                       if (y==0){
                                y=8;
                               if (letter==0)
                                        letter=length-2;
                                else
                                        letter--;
                        }
                        else{
                                y--;
                        }
                        if (letter>0){
                                selectDevice(file,MATRIX1);
                                for(i=0; i<8; i++){
                                        h_bitShifted[i] = (contiguousText[i][letter]) << y | (contiguousText[i][letter+1]) >> (8-y);
                                        h_bitShifted[i]  = reverseBits(h_bitShifted[i]);
                                }
                                displayImage(h_bitShifted,res, daddress, file);
                        }

                        if (letter-1>0){
                                selectDevice(file,MATRIX2);
                                for(i=0; i<8; i++){
                                        h_bitShifted[i] = (contiguousText[i][letter-1]) << y | (contiguousText[i][letter]) >> (8-y);
                                        h_bitShifted[i] = reverseBits(h_bitShifted[i]);
                                }
                                displayImage(h_bitShifted,res, daddress, file);
                        }

                        if (letter-1>1){
                                selectDevice(file,MATRIX3);
                                for(i=0; i<8; i++){
                                        h_bitShifted[i] = (contiguousText[i][letter-2]) << y | (contiguousText[i][letter-1]) >> (8-y);
                                        h_bitShifted[i] = reverseBits(h_bitShifted[i]);
                                }
                                displayImage(h_bitShifted,res, daddress, file);
                        }
                }
                        horizontalTimer = mymillis();
        }
        }

}

void selectDevice(int file, int addr)
{
        char device[3];
        if (addr == 0x70)
                device == "MATRIX1";
        else if (addr == 0x71)
                device == "MATRIX2";
        else
                device == "MATRIX3";


        if (ioctl(file, I2C_SLAVE, addr) < 0) {
               fprintf(stderr,
                        "Error: Could not select device 0x%02x: %s\n",
                        device, strerror(errno));
        }
}

int mymillis()
{
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec) * 1000 + (tv.tv_usec)/1000;
}



