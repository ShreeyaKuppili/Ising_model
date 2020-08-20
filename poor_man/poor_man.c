#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include "redpitaya/rp.h"

#define M_PI 3.14159265358979323846

int main (int argc, char **argv) 
{
    FILE *fp;
    fp = fopen("./out_data/x_n.csv", "w+");
    // Initialization of API
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "Red Pitaya API init failed!\n");
        return EXIT_FAILURE;
    }

    // Buffer size
    uint32_t buff_size = 16;
    
    // Initialise variables

    // x_k is the "current" value of the photovoltage
    float x_k = 0.0;

    // x_n an array that will store the output
    float *x_n = (float *)malloc(buff_size * sizeof(float));

    // buff stores the input
    float *buff = (float *)malloc(buff_size * sizeof(float));

    float next;

    int i;
    rp_acq_trig_state_t state;

    rp_GenWaveform(RP_CH_2, RP_WAVEFORM_ARBITRARY);
    rp_GenAmp(RP_CH_2, 1);

    // Enable burst mode
    rp_GenMode(RP_CH_2, RP_GEN_MODE_BURST);
    // One waveform per burst
    rp_GenBurstCount(RP_CH_2, 1);
    // Number of bursts
    rp_GenBurstRepetitions(RP_CH_2, -1);
    // Burst period. Will be dependent on computation time
    rp_GenBurstPeriod(RP_CH_2, 5000);

    rp_AcqReset();
    rp_AcqSetDecimation(1);
    rp_AcqSetTriggerDelay(0);

    rp_AcqStart();

    // After acquisition is started some time delay is needed in order to 
    // acquire fresh samples in to buffer
    // Here we have used time delay of one second but you can calculate exact
    // value taking in to account buffer length and smaling rate

    sleep(1);

    while(1)
    {
        rp_AcqSetTriggerSrc(RP_TRIG_SRC_NOW);
        state = RP_TRIG_STATE_TRIGGERED;

        do
        {
            rp_AcqGetTriggerState(&state);
            printf("State = %d\n", state);
        }while(state != RP_TRIG_STATE_TRIGGERED);

        // Get data into buff
        rp_AcqGetOldestDataV(RP_CH_1, &buff_size, buff);

        // Average over the buffer size
        for(i = 0; i < buff_size; i++)
        {
            //printf("%f\n", buff[i]);
            x_k += buff[i];
        }
        x_k /= buff_size;

        printf("x_k = %f \n", x_k);

        // Calculate the next value according to the equation
        next = pow(cos(x_k - (0.25*M_PI)),2) - 0.5;

        // Store the value in the buffer to be given as output for the next
        // buff_size cycles
        for(i = 0;i < buff_size; i++)
        {
            x_n[i] = next;
	    //printf("x_n: %f \n",x_n[i]);
        }

        // Print the value calculated.
        printf("next: %f \n", next);
        fprintf(fp, "%f\n", next);

        // Send the output
        rp_GenArbWaveform(RP_CH_2, x_n, buff_size);

        // rp_GenTrigger(RP_CH_2);
        rp_GenOutEnable(RP_CH_2);
    }

    // Releasing resources
    free(x_n);
    free(buff);
    rp_Release();
    fclose(fp);

    return EXIT_SUCCESS;
}
