#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>

#include "redpitaya/rp.h"

#define M_PI 3.14159265358979323846
#define N_iters 100
#define N 10
#define alpha_max 2

void gen_noise(float *noise)
{
	int i = 0;
	srand(time(0));
	float mu = 0.0, sig = 0.0;
	for(;i<N_iters;i++)
	{
		noise[i] = rand()%256;
		mu += noise[i];
		sig += noise[i]*noise[i];
	}
	mu /= N_iters;
	sig /= N_iters;
	sig -= mu*mu;
	sig = sqrt(sig)*20;
	for(i=0;i<N_iters;i++)
	{
		noise[i] = (noise[i] - mu)/sig;
	}
}

int main (int argc, char **argv) 
{
    FILE *fp;
    fp = fopen("./out_data/bifurcation_constant_outside_cosine.csv", "w+");
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
    float *zero = (float *)malloc(N_iters * sizeof(float));

    // buff stores the input
    float *buff = (float *)malloc(buff_size * sizeof(float));

    float *noise = (float *)malloc(N_iters * sizeof(float));
    gen_noise(noise);
     	
    int i;
    for(i=0;i<N_iters;i+=10)
    {
	    printf("Noise %d %f\n",i,noise[i]);
    }
   
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
    
    float alpha;
    int j;
    int count=0;
    for(alpha=-1;alpha<=alpha_max;alpha+=0.01)
    {
        fprintf(fp, "\nAlpha = %f\n", alpha);
        for(j=0;j<N;j++)
        {
	    fprintf(fp, "Run number = %d \n",j);
            float next = 0.0, old_next = 1.0;
	    count = 0;
            while(count<N_iters)
            {
		count++;
                old_next = next;
                rp_AcqSetTriggerSrc(RP_TRIG_SRC_NOW);
                state = RP_TRIG_STATE_TRIGGERED;
                int f = 0;

                do
                {
                    rp_AcqGetTriggerState(&state);
                    //printf("\nState = %d\n", state); 
                    f++;
                    if(f > 20)
                    {
			printf("\n Acquisition not Triggered \n");
			f = 0;
                        //exit(0);
                    }
                }while(state != RP_TRIG_STATE_TRIGGERED);

                // Get data into buff
                rp_AcqGetLatestDataV(RP_CH_1, &buff_size, buff);

                // Average over the buffer size
                for(i = 0; i < buff_size; i++)
                {
                    //printf("%f\n", buff[i]);
                    x_k += buff[i];
                }
                x_k /= buff_size;
		x_k -= 0.4;
		fprintf(fp,"%d %f \n",count,x_k);

                //printf("x_k = %f \n", x_k);
		
		next = alpha*x_k + noise[count%N_iters];
                // Calculate the next value according to the equation
                //next = pow(cos(alpha*x_k - (0.25*M_PI) + noise[%N_iters]),2) - 0.5;

                // Store the value in the buffer to be given as output for the next
                // buff_size cycles
                for(i = 0;i < buff_size; i++)
                {
                    x_n[i] = next;
                }
		if(count == N_iters - 2)
		//Storing the penultimate value
		{
			//fprintf(fp, "%f\n", next);
		}
                // Print the value calculated.
                //printf("next: %f \n", next);
                // printf("Old next = %f\n",old_next);
                // printf("Diff = %f\n", fabs(old_next-next));
                // Send the output
                if(rp_GenArbWaveform(RP_CH_2, x_n, buff_size)!=RP_OK)
		{
			printf("Exceeded the maximum possible output, Alpha = %f, next = %f \n",alpha,next);
			//exit(0);
		}

                // rp_GenTrigger(RP_CH_2);
                rp_GenOutEnable(RP_CH_2);
            }
            //fprintf(fp, "%f\n", next);
	    printf("Alpha = %f Final value: %f \n",alpha,next);		
	    //printf("____________________________");
            for(i = 0;i < buff_size; i++)
            {
                x_n[i] = 0.0;
            }
            rp_GenOutDisable(RP_CH_2);
            rp_GenArbWaveform(RP_CH_2, zero, buff_size);

            // rp_GenTrigger(RP_CH_2);
            rp_GenOutEnable(RP_CH_2);
        }
    }

    // Releasing resources
    free(x_n);
    free(buff);
    rp_GenOutDisable(RP_CH_2);
    rp_AcqStop();
    rp_Release();
    fclose(fp);

    char cmd[] = "python3 ./helper_functions/plotter.py h ./out_data/bifurcation.csv";
    // system(cmd);

    return EXIT_SUCCESS;
}
