#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <inttypes.h>
#include <pthread.h>

#include "redpitaya/rp.h"

#define M_PI 3.14159265358979323846

int N_iters = 50;
int N_noise = 100;
int N_spins = 1;
int buff_size = 16;

float ALPHA_MAX = 3.0;
float ALPHA_MIN = 0.5;
float ALPHA_STEP = 0.1;
float offset = 0.04;
float sig_f = 1/40;
float scale = 20;

void *acquisition_handler(void *);

pthread_t acquisition_thread;

// Buffer size
//const uint32_t buff_size = 16;

// x_in stores the input
volatile float *x_in;
float *noise;
volatile float *x_out;
FILE *fp;
float x_k;

void gen_noise()
{
    // Generate noise with 0 mean and some deviation. Deviation can be changed
    // at run time
	int i = 0;
	srand(time(0));
	float mu = 0.0, sig = 0.0;
	for(;i<N_noise;i++)
	{
		noise[i] = rand()%64;
		mu += noise[i];
		sig += noise[i]*noise[i];
	}
	mu /= N_noise;
	sig /= N_noise;
	sig -= mu*mu;
	sig = sqrt(sig)/sig_f;
	for(i=0;i<N_noise;i++)
	{
		noise[i] = (noise[i] - mu)/sig;
	}
}

void *acquisition_handler(void *dummy)
{
    rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;    

    // Wait for buffer to get full after Trigger.
    do
    {
        rp_AcqGetTriggerState(&state);
        //printf("State = %d\n", state);
    }while(state == RP_TRIG_STATE_TRIGGERED);

    uint32_t b_size = buff_size;
    // Get data into buff
    rp_AcqGetLatestDataV(RP_CH_1, &b_size, x_in);
}

void single_iteration(float alpha, int s,int iteration)
{
    int i;
    //compute x_out

    int n = rand()%N_noise;
    // Multiiply by alpha and add noise
    float next = alpha*x_k+noise[n];
   
    //Threshold the output
    if(next>=1.0)
	next = 1.0;
    else if(next<=-1.0)
	next = -1.0;

    // Calculate the next value according to the equation

    // next = pow(cos(next + (0.25*M_PI)),2); //Modulator function. 
    // Remove in lab

    // x_out an array that will store the output
    
    // Store the value in the buffer to be given as output for the next
    // buff_size cycles
    for(i = 0;i < buff_size; i++)
    {
        x_out[i] = next;
    }
    printf("next = %f\n\n", next);


    // Send the output
    rp_GenArbWaveform(RP_CH_2, x_out, buff_size);
    // Enable burst mode
    rp_GenMode(RP_CH_2, RP_GEN_MODE_BURST);
    // One waveform per burst
    rp_GenBurstCount(RP_CH_2, 1);
    // Number of bursts
    rp_GenBurstRepetitions(RP_CH_2, -1);
    // Burst period. Will be dependent on computation time
    // rp_GenBurstPeriod(RP_CH_2, 5000);

    rp_GenAmp(RP_CH_2, 0.9);
    // rp_GenFreq(RP_CH_2, 4000.0);

    rp_AcqReset();
    // Start acquisition
    rp_AcqStart();

    // Trigger immediately
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_NOW);

    // Start the acquisition thread
    pthread_create(&acquisition_thread, NULL, acquisition_handler, NULL);
    rp_GenOutEnable(RP_CH_2);

    // Wait for acquisition to complete
    pthread_join(acquisition_thread,NULL);

    // Stop acquisition
    rp_AcqStop();

    //Reset the output to zero
    rp_GenAmp(RP_CH_2, 0);
    
    for(i=0;i<buff_size;i++)
	printf("x_out[%d]= %f \n",i,x_out[i]);

    x_k = 0.0;   
    // Average over the buffer size
    for(i = 0; i < buff_size; i++)
    {
      	printf("x_in[%d] = %f \n",i,x_in[i]);
   	x_k += x_in[i];
    }

    // Add the offset
    x_k /= buff_size;
    x_k -= (offset);
    x_k *= scale;

    fprintf(fp,"%f %d %d %f\n",alpha,s,iteration,x_k);

}


int main (int argc, char **argv) 
{

    fp = fopen("data.csv","w");
    //fprintf(fp,"Writing a test line\n");
    x_out = (float *)malloc(buff_size * sizeof(float));
    x_in = (float *)malloc(buff_size * sizeof(float));
    noise = (float *)malloc(N_noise * sizeof(float));
    fprintf(fp,"# Alpha Run/Spin Iteration Value\n");
    // Initialization of API
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "Red Pitaya API init failed!\n");
        return EXIT_FAILURE;
    }

    int i,s;
    float alpha;
    gen_noise();
    //initialize x_out to zero
    for(i=0;i<buff_size;i++)
	x_out[i] = noise[i];

    rp_AcqReset();
    rp_AcqSetDecimation(1);

    // Think we might have to change this
    rp_AcqSetTriggerDelay(8200);

    rp_GenWaveform(RP_CH_2, RP_WAVEFORM_ARBITRARY);
        
    if(argc > 1)
    {
        for(int a=1;a<argc;a++)
        {
            char opt = argv[a][0];
            switch(opt)
            {
                case 'i': // Number of iterations per spin
                        N_iters =atoi(argv[++a]);
                        break;
                case 'v': // Standard deviation of noise
                        sig_f = atof(argv[++a]);
                        break;
                case 'u': // Upper limit of alpha
                        ALPHA_MAX = atof(argv[++a]);
                        break;
                case 'p': // Step size of alpha
                        ALPHA_STEP = atof(argv[++a]);
                        break;
                case 'd': // Lower limit of alpha
                        ALPHA_MIN = atof(argv[++a]);
                        break;
                case 'a': // If we want only one value of alpha
                        ALPHA_MIN = atof(argv[++a]);
                        ALPHA_MAX = ALPHA_MIN;
                        break;  
                case 'N': // Number of spins/runs
                        N_spins = atoi(argv[++a]);
                        break;
                case 'o': // Offset
                        offset = atof(argv[++a]);
                        break;
                case 's': // Scaling factor due to the photodiode
                        scale = atof(argv[++a]);
                        break;
                case 'n': // Number of points in noise
                        N_noise = atoi(argv[++a]);
                        break;
                case 'b': // Buffer size of x_in and x_out
                        buff_size = atoi(argv[++a]);
                        break;
                case 'f': // File to save data in
                        fclose(fp);
                        fp = fopen(argv[++a],"w");
			            break;
                default:printf("Invalid option.\n");
			            return 0;
            }
        }
    }
    //printf("Alpha MAx = %f\nAlpha min= %f\nalpha step =  %f\n offset=%f\n   \
    N_iters= %d\nN_spins= %d\n buff_size=%d\n",ALPHA_MAX,ALPHA_MIN, ALPHA_STEP,\
    offset,N_iters,N_spins,buff_size);
   
    for(alpha = ALPHA_MIN;alpha <= ALPHA_MAX;alpha += ALPHA_STEP)
    {
	// Generate new noise for each alpha
        gen_noise();
        for(s = 0;s < N_spins; s++)
        {
	    x_k = 0.0;
            for(i = 0;i < N_iters;i++)
            {
                single_iteration(alpha,s,i);
            }
        }
    }

    // Releasing resources
    free(x_out);
    free(x_in);
    free(noise);
    rp_GenOutDisable(RP_CH_2);
    rp_Release();
    fclose(fp);
    return EXIT_SUCCESS;
}
