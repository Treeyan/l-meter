
//
// 作者 : Treeyan  11/5/2022
// 编译环境 : sdcc
//
// LC振荡电感表
// 
// 1/( 2*pi*sqrt(L*C) ) = 频率. 固定的 LC 值可获得稳定的振荡频率，
// 当 L串接待测电感时，振荡频率下降，通过 c51 计数器得到频率变化值，
// 从而计算出待测电感量。嗯，不适宜测量品质因数过低的电感
//

#include <8052.h>
#include <math.h>
#include <stdio.h>		// sprintf
#include <string.h>

typedef unsigned char _u8_t;
typedef unsigned int  u16_t;
typedef unsigned long u32_t;
typedef char		  _bool;

#define _nop_()  __asm nop __endasm

__sfr __at (0x8E)   AUXR;						// STC89C52 ALE 寄存器地址

enum { false, true };
enum { start, stop };

#define d1602_RS    			P1_1
#define d1602_RW    			P1_0
#define d1602_EB    			P1_2
#define d1602_BZ    			P0_7
#define D1602_RG				P0

//
// 12T 模式
//
#define MODE_T                  (12)                            // 12T 模式
#define FOSC					(24L*1000*1000)                 // 24MHZ crystal.
#define NC_PERSC				(40)                            // 16位计数器在当前晶振频率下每秒需要执行的次数
#define NC_COUNT				(65536-(FOSC/MODE_T/NC_PERSC))	// 1000/NC_PERSC = 25毫秒所裝填的计数值.
#define MS_COUNT                (FOSC/MODE_T/1000)              // 每毫秒可以执行的单周期指令数

//
//
//
#define COUNT_RELOAD() {            \
    TL0 = (_u8_t)( NC_COUNT );      \
    TH0 = (_u8_t)( NC_COUNT >> 8 ); \
}

#define TIMER_STOP()    {   \
	Update = true;          \
    TR0 = 0;                \
	ET0 = 0;                \
	TR1 = 0;                \
	ET1 = 0;                \
}

// 标识计数开始

#define TIMER_START()   {       \
    Update = false;             \
    /* 重置计数器和定时器. */    \
	OneSecond = NC_PERSC;       \
	Frequency = 0;              \
    COUNT_RELOAD();             \
	TR0 = 1;                    \
	ET0 = 1;                    \
                                \
	TL1 = 0;    \
	TH1 = 0;    \
	TR1 = 1;    \
	ET1 = 1;    \
}

//
//
//
_u8_t  OneSecond;
u32_t  Frequency;
_bool  DoZero;
_bool  Update;

//
//
//
void delay( _u8_t n )
{
	_u8_t i;
	for(i = 0; i < n; i++)
		_nop_();
}

void Sleep( u32_t ms )
{
	u16_t i;
	u32_t j;

    //
    // 假定这个双循环有60条单周期指令，乱猜的 :)
    // 
    for(j = 0; j < ms; j++) {
		for(i = 0; i < MS_COUNT/60; i++) {
			_nop_();
		}
	}
}

void d1602Idle()
{
	int busy;

	do {		 

		P0 = 0xff;
		d1602_RS = 0;
		d1602_RW = 1;
		d1602_EB = 1;
		delay(5);
		busy = d1602_BZ;
		d1602_EB = 0;

	}while( busy );
}

void d1602DoCmd( _u8_t command )
{		
	d1602Idle();

	d1602_RS = 0;
	d1602_RW = 0;
	d1602_EB = 0;	

	D1602_RG = command;

	delay(2);
	d1602_EB = 1;
	delay(2);
	d1602_EB = 0;
}

void d1602PutChar( unsigned char dsc )
{
	d1602Idle();

	d1602_RS = 1;
	d1602_RW = 0;
	d1602_EB = 0;

	D1602_RG = dsc;

	delay(2);
	d1602_EB = 1;	 
	delay(2);
	d1602_EB = 0;
}

void ExternalZeroIsr() __interrupt 0
{
    DoZero = true; // 外部置 0 按键按下
}

void LmTimerIsr() __interrupt 1
{
	if( 0 == (--OneSecond) ) {
	
		TIMER_STOP();
        return;	
	} 
	//
	// 继续计数
	//
    COUNT_RELOAD();
}

void LmCounterIsr() __interrupt 3
{
	TL1 = 0;
	TH1 = 0;

	Frequency += 65536;
}

char oscsc[16*2];	//  包含终止字符 0 

void d1602PutString( const char* ps )
{
	_u8_t i;

	d1602DoCmd( 0x01 ); // 清屏
	d1602DoCmd( 0x80 ); // 移动到第一行
	
    //
    // 1602 最大显示32个字符
    // 
	for(i = 0; ps[i] && i < 32 ; i++) { 
	
		d1602PutChar( ps[i] );
	
		if( 15 != i ) 
            continue;

		d1602DoCmd( 0xC0 ); // 移动到第二行
	}
}

void d1602PutLineString( const char * ps, _u8_t i )
{
	if( 0 == i )
		d1602DoCmd( 0x80 ); // 移动到第一行
	else
		d1602DoCmd( 0xC0 ); // 移动到第二行
		
	for( i = 0; i < 16; i++ ) {
		if( *ps ) {
			d1602PutChar( *ps );
			ps++;
		} else {
			d1602PutChar( ' ' );
		}
	}					
}

//
// sdcc 系统默认分发版本 sprintf 不能处理浮点数，所以这里分割整数和小数部分
//
void c_sdcc_split_float( float v, long * integer, int * remaind ) 
{   
    *integer = ( long )v;
    *remaind = ( int  )(( long )( v * 100 ) % 100 );
}

void c_sdcc_float_sprintf( char * out, char * in, float v )
{
    long    integer;
    int     remaind;

    c_sdcc_split_float( v, &integer, &remaind );   
    sprintf( out, in, integer, remaind );
}

//
// main，主处理流程
//
void main()
{
    float 	inductance;
    float 	value;
    float 	inducConst = 2200.0f;   // uh 实估值，计算时以微亨为基本单位
    float 	capacConst = 0.00118f;  // uf 实估值, 计算时以微法为基本单位

    u16_t 	countV;
	_bool	verify;
   
    AUXR = 1; // 关闭 ALE，传说可以减少电磁辐射，测不着，不清楚

	d1602Idle();	
	d1602DoCmd( 0x01 ); // 清屏.
	d1602DoCmd( 0x38 ); // 5x7 阵列.
	d1602DoCmd( 0x0c ); // 不显示光标, 开显示.
	d1602DoCmd( 0x06 ); // 自动光标.

	d1602PutString( "    L-METER    " );
    Sleep( 100 );

    DoZero    = false;
	OneSecond = NC_PERSC;
	Frequency = 0;
	Update	  = false;
	verify	  = false;
    
    // 
    // 一个内部计时器模式，一个外部计数器模式
    //
	TMOD = 0x50 | 0x1;

    IT0 = 0;    // 低电平触发.
    EX0 = 1;    // 使能外部中断.

	TIMER_STOP();

	EA	= 1;	// 开全局中断

	//
	// 主流程
	//

	TIMER_START();
                                                     
	while( true ) {

		if( false == Update ) {
            continue;
        }

		countV = TH1;
		countV = ( countV << 8 ) + TL1;

		Frequency += countV;

        if( 0 < Frequency ) {
            //
            // 1 / ( 2*pi*sqrt( L*C )) = frequency.
            //
		    value =  1 / 6.2832f / ( Frequency / 1000000.0f );
		    value *= value; // value 的平方.
		    inductance = value / capacConst;
        
        } else {

            inductance = 0.0f;
        }

       	d1602DoCmd( 0x01 ); // 清屏.

        if( true == DoZero ) {
            /*
              外部中断触发，计数器数值会有较大误差，下一次循环计算清零值
            */
			d1602PutLineString("  BUTTON DOWN ", 0 );
			d1602PutLineString("    Zero C ", 1 );

            Sleep( 1000 );

            DoZero = false;            
			verify = true;

            TIMER_START();
            continue;
        }

		sprintf( oscsc, " F: %l4d.%l03d Khz ", Frequency / 1000, Frequency % 1000 );
		d1602PutLineString( oscsc, 0 );

        if( true == verify ) {

            if ( 0.0f == inductance ) {
                strcpy( oscsc, " Z-C: Error. " );
            } else if ( inductance == inducConst ){
                strcpy( oscsc, " Z-C: SAME. " );
            } else {
                long    integer; 
                int     remaind;
                float   deviati;

                deviati = inductance - inducConst;    
                /* 计算电容误差值 */
                deviati = ( deviati / inducConst ) * capacConst;
                capacConst += deviati;
                c_sdcc_split_float( capacConst * 1000000, &integer, &remaind );
                sprintf( oscsc, " Z-C:%l4d.%02d pF ", integer, remaind );
            }

			d1602PutLineString( oscsc, 1 );

            Sleep( 1000 );

			verify = false;
		
		} else {

			inductance = inductance - inducConst;

			if( 0.01f > inductance ) {       
				inductance = 0.0f;
			}

            if(  1100.0f > inductance ) {
                c_sdcc_float_sprintf( oscsc, " L: %l4d.%02d  uH ", inductance );
            } else if( 1001000.0f > inductance ) {
                inductance /= 1000.0f;
                c_sdcc_float_sprintf( oscsc, " L: %l4d.%02d  mH ", inductance );
            } else {
                sprintf( oscsc, " L:  OVER.  ", inductance );
            }

			d1602PutLineString( oscsc, 1 );
        }

		TIMER_START();
	}
}
