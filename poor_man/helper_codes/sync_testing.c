#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#include <inttypes.h>
#include <pthread.h>

#include "redpitaya/rp.h"

#define M_PI 3.14159265358979323846
#define BUFFER_SIZE 16*1024

int p_step = 1000;
int trig_delay;
int t1 =  = 16384+3900;
int t2 = 3900+16384+1200;
int breps = 3;
int bcounts = 1;
float freq = 7690.0;

int N_spins = 1;
int N_iters = 30;
int N_noise = 100;
int N_runs = 1;
int buff_size = BUFFER_SIZE;
int buff_per_spin;

float ALPHA_MAX = 1.5;
float ALPHA_MIN = 1.5;
float ALPHA_STEP = 0.1;

float BETA_MAX = 0.5;
float BETA_MIN = 0.5;
float BETA_STEP = 0.1;

float offset = 0.5;
float sig_f = 0.04;
float scale = 1;
float att = 0.9;

void single_iteration(float, float, int, int);

// x_in stores the input
float *x_in;
float *x_k;
float *x_out;

FILE *fp, *fp2, *j_file;
float *noise;

void single_iteration(float alpha, float beta, int s,int iteration)
{
    int i, n;

    //compute x_out

    for(i = 0;i < N_spins;i++)
    {
        n = rand()%N_noise;
        next[i] = 0.01*n;

        //Threshold the output
        if(value >= 1.0)
        {
            value = 1.0;
        }
        else if(value<=-1.0)
        {
            value = -1.0;
        }
        
	    // printf("Value = %f\n", value);

        // x_out an array that will store the output
    
        // Store the value in the buffer to be given as output for the next
        // buff_size cycles
        for(int j = i*buff_per_spin;j < (i+1)*(buff_per_spin);j++)
        {
            x_out[j] = value;
        }
    }

    // Send the output
    rp_GenWaveform(RP_CH_2, RP_WAVEFORM_ARBITRARY);
    rp_GenArbWaveform(RP_CH_2, x_out, buff_size); //Does it start generating here itself?
    // Enable burst mode
    rp_GenMode(RP_CH_2, RP_GEN_MODE_BURST); 
    // One waveform per burst
    rp_GenBurstCount(RP_CH_2, bcounts);
    // Number of bursts
    rp_GenBurstRepetitions(RP_CH_2, breps);
    // Burst period. Will be dependent on computation time
    //rp_GenBurstPeriod(RP_CH_2, 130000);

    //rp_GenAmp(RP_CH_2, 1.0);
    rp_GenFreq(RP_CH_2, freq);

    rp_AcqReset();
    // Start acquisition
    rp_AcqSetTriggerDelay(trig_delay);
    rp_AcqStart();

    // Trigger immediately
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_NOW);
    int pos;
    rp_AcqGetWritePointerAtTrig(&pos);

    //printf("Pos right after trigger now:%d\n",pos);

    rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;    

    // Wait for buffer to get full after Trigger.
    do
    {
        rp_AcqGetTriggerState(&state);
	    rp_AcqGetWritePointer(&pos);
        //printf("Pos = %d\n", pos);
    }while(state == RP_TRIG_STATE_TRIGGERED);
	
    rp_AcqGetWritePointer(&pos);
    //printf("Pos = %d\n",pos);
    // Get data into buff
    rp_AcqGetOldestDataV(RP_CH_1, &buff_size, x_in);

    // Stop acquisition
    rp_AcqStop();

    //Reset the output to zero
    //rp_GenOutDisable(RP_CH_2);
    //rp_GenReset();

    trig_delay = t2;

    for(i=0;i<buff_size;i+=p_step)
    {
	    // printf("x_out[%d]= %f \n",i,x_out[i]);
    }

    for(i = 0; i < buff_size; i+=p_step)
    {
	    // printf("x_in[%d] = %f \n",i,x_in[i]);
    }

    for(i=0;i<buff_size;i++)
    {
	    fprintf(fp,"iter=%d %d %f %f\n",iteration,i,x_out[i],x_in[i]);
    }
    // int shift = find_shift();
    // printf("Best correlation value received was for shift of = %d",shift);
}

int main (int argc, char **argv) 
{
    system ("cat /opt/redpitaya/fpga/fpga_0.94.bit > /dev/xdevcfg");
    struct tm *timenow;
    char sync_filename[40], traj_filename[50];

    time_t now = time(NULL);
    timenow = localtime(&now);
    strftime(sync_filename, sizeof(sync_filename), "./../data/xout_xin_%d_%m_%Y_%H_%M_%S.csv", timenow);

    char comment[100];
    printf("Enter comment on file: ");
    fgets(comment, sizeof(comment), stdin);  // read string

    fp = fopen(sync_filename,"w");

    fprintf(fp, "#%d\n",10);
    printf("%s\n", comment);

    if(argc > 1)
    {
        for(int a=1;a<argc;a++)
        {
            char opt = argv[a][1];
            switch(opt)
            {
		case 'N': //Number of spins
			N_spins = atoi(argv[++a]);
			break;
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
                case 'l': // Lower limit of alpha
                        ALPHA_MIN = atof(argv[++a]);
                        break;
                case 't': // Upper limit of beta
                        BETA_MAX = atof(argv[++a]);
                        break;
                case 'g': // Step size of beta
                        BETA_STEP = atof(argv[++a]);
                        break;
                case 'd': // Lower limit of beta
                        BETA_MIN = atof(argv[++a]);
                        break;
                case 'a': // If we want only one value of alpha
                        ALPHA_MIN = atof(argv[++a]);
                        ALPHA_MAX = ALPHA_MIN;
                        break;  
                case 'b': // Change value of beta
                        BETA_MIN = atof(argv[++a]);
                        BETA_MAX = BETA_MIN;
                        break;
                case 'j': //Change value of J
                        fclose(j_file); 
                        j_file = fopen(argv[++a],"r");
 			char line_for_N[20];
			fgets(line_for_N, 20, j_file);
                        sscanf(line_for_N, "%d %*d", &N_spins);
                        break;
                case 'r': // Number of spins/runs
                        N_runs = atoi(argv[++a]);
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
                case 'f': // File to save data in
                        fclose(fp);
                        fp = fopen(argv[++a],"w");
			            break;
                case 'F':
                        freq = atof(argv[++a]);
                        break;
                case 'B':
                        if(argv[a][2] == 'c')
                            bcounts = atoi(argv[++a]);
                        else
                            breps = atof(argv[++a]);
                        break;
                        
                case 'T': //trig_delay
                        if(argv[a][2] == '1')
                            t1 = atof(argv[++a]);
                        else
                            t2 = atof(argv[++a]);
                        break;
                        
                default:printf("Invalid option: %c\n",opt);
			            return 0;
            }
        }
    }

    x_out = (float *)calloc(buff_size, sizeof(float));
    x_in = (float *)calloc(buff_size, sizeof(float));

    int i,s;

    // Initialization of API
    if (rp_Init() != RP_OK) 
    {
        fprintf(stderr, "Red Pitaya API init failed!\n");
        return EXIT_FAILURE;
    }
    else
    {
	printf("Initialisation of RedPitaya API done!\n");
    }

    float alpha;
    float beta;

    rp_AcqReset();
    rp_AcqSetDecimation(1);

    rp_GenOutEnable(RP_CH_2);

    buff_per_spin = (int)BUFFER_SIZE/N_spins;
   
    for(alpha = ALPHA_MIN;alpha <= ALPHA_MAX;alpha += ALPHA_STEP)
    {
        for(beta = BETA_MIN;beta <= BETA_MAX;beta += BETA_STEP)
        {
            for(s = 0;s < N_runs; s++)
            {
                for(i = 0;i < N_iters;i++)
                {
                    single_iteration(alpha, beta, s, i);
                }
	        printf("Alpha = %f Beta = %f The cut value is: %f\n",alpha,beta, cut_value());
            }
        }
    }

    // Releasing resources
    free(x_out);
    free(x_in);
    rp_Release();
    fclose(fp);
    return EXIT_SUCCESS;
}
