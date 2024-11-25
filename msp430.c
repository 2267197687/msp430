#include <msp430.h>

typedef unsigned int bool;
#define true 1
#define false 0

#define BAUDRATE 9600
#define REF_VOLTAGE 3.0 // 假设参考电压为3.0V

// 定义采样模式全局变量
volatile bool SingleFlag = false;
volatile bool ContinuousFlag = false;

void UART_Init(void) {
    P3SEL |= BIT1 + BIT0; // 选择P3.1和P3.0作为UART TXD和RXD
    UCB1CTL1 |= UCSWRST; // 进入复位状态
    UCB1CTL0 = UCCKPH | UCMST | UCSYNC; // 3线UART模式，主模式，同步模式
    UCB1CTL1 = UCSSEL_2; // 使用SMCLK作为时钟源
    UCB1BR0 = 104; // 设置波特率为9600
    UCB1BR1 = 0;
    UCB1MCTL = UCBRS2 + UCBRS0; // 计算波特率
    UCB1CTL1 &= ~UCSWRST; // 释放复位状态
}

void ADC_Init(void) {
    WDTCTL = WDTPW + WDTHOLD; // 停止看门狗计时器
    ADC10CTL0 = SREF_1 + ADC10SHT_3 + REFON + ADC10ON + ADC10IE; // 选择内部参考电压，开启ADC
    ADC10CTL1 = INCH_0 + ADC10DIV_1 + CONSEQ_0; // 选择通道0，设置采样时间，单次采样模式
}

void Timer_Init(void) {
    TACTL = TASSEL_2 + MC_1 + ID_3; // 使用SMCLK，上溢模式，分频8
    TACCR0 = 62500 - 1; // 设置定时器周期
    TACCTL0 = CCIE; // 使能定时器中断
}

void UART_SendByte(unsigned char byte) {
    while (!(IFG2 & UCB1TXIFG)); // 等待上一个字节发送完成
    UCB1TXBUF = byte; // 发送一个字节
}

void UART_SendInt(int value) {
    unsigned char buffer[5];
    sprintf(buffer, "%d", value);
    for (int i = 0; buffer[i] != '\0'; i++) {
        UART_SendByte(buffer[i]);
    }
}

void ADC_Sample(void) {
    if (SingleFlag) {
        ADC10CTL0 |= ADC10SC; // 启动单次ADC采样
        SingleFlag = false; // 重置标志位
    }
}

int main(void) {
    WDTCTL = WDTPW + WDTHOLD; // 停止看门狗计时器
    BCSCTL1 = CALBC1_1MH; // 设置DCO为1MHz
    DCOCTL = CALDCO_1MH; // 设置DCO为1MHz

    UART_Init(); // 初始化UART
    ADC_Init(); // 初始化ADC
    Timer_Init(); // 初始化定时器

    __enable_interrupt(); // 使能全局中断

    while(1) {
        ADC_Sample(); // 执行ADC采样
    }
}

#pragma vector = UCB1RX_VECTOR
__interrupt void UART_Rx_ISR(void) {
    unsigned char receivedByte = UCB1RXBUF; // 读取接收到的字节
    if (receivedByte == 'S') {
        SingleFlag = true;
        ContinuousFlag = false;
    } else if (receivedByte == 'C') {
        SingleFlag = false;
        ContinuousFlag = true;
    }
    UCB1RXBUF &= ~UCB1RXIFG; // 清除接收中断标志
}

#pragma vector = ADC10_VECTOR
__interrupt void ADC10_ISR(void) {
    if (ADC10IFG) { // 检查ADC中断标志
        unsigned int adcValue = ADC10MEM; // 读取ADC结果
        int voltage = (adcValue * REF_VOLTAGE) / 1024; // 计算电压值
        // 发送数据到PC
        UART_SendInt(voltage);
        UART_SendByte('\n');
        ADC10IFG = 0; // 清除ADC中断标志
    }
}

#pragma vector = TIMER0_A0_VECTOR
__interrupt void Timer_A(void) {
    TACTL = MC_1 + ID_3; // 重新加载定时器
    if (ContinuousFlag) {
        ADC10CTL0 |= ADC10SC; // 启动ADC采样
    }
}