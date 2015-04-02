void ADC_Init(uint32_t channelNum);

void ADC_Open(uint32_t channelNum, uint32_t period); // opens a channel 0-11

uint32_t ADC_In(void); // reads from the adc

int ADC_Collect(uint32_t channelNum , uint32_t fs, // fs is frequency
 void (*task)(unsigned short));

int ADC_Status(void); // return 0 when done


