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
int trig_delay = 16384+3900; // + 500
int t1 = 7830;	//16384+3900;
int t2 = 8130;	//3900+16384+1200;
int breps = 1; 	// 3
int bcounts = 1;
float freq = 7630.0;

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

void gen_noise();
void read_J();
void feedback(float *, float, float);
void single_iteration(float, float, int, int);
float cut_value();
double correlation(int);
int shift();

// x_in stores the input
float *x_in;
float *x_k;
float *x_out;

FILE *fp, *fp2, *fp3, *j_file;
float *noise;
float **J;

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
		// printf("\nNoise %d = %f\n",i ,noise[i]);
	}
}

void read_J()
{
    int row,column;
    float value;
    char line[20];
    printf("\nReading file for J...\n");
    
    while(fgets(line, 20, j_file)!=0)
    {
        // puts(line);
        sscanf(line, "%d %d %f", &row, &column, &value);
        // printf("J[%d][%d]=%f\n",row,column,J[row][column]);
        J[row-1][column-1] = value;
	J[column-1][row-1] = value;
    }
    fclose(j_file);
}

void feedback(float *fb, float alpha, float beta)
{
    for(int i = 0;i < N_spins;i++)
    {
        for(int j = 1;j < N_spins;j++)
	    {
            fb[i] += J[i][(i+j)%N_spins]*x_k[(j+i)%N_spins];
	    }
	    fb[i] *= -beta;
        fb[i] += alpha*x_k[i];
    }
}

double correlation(int shift)
{
    double cor = 0.0;
    double norm_out = 0.0, norm_in = 0.0;
    float *x_in_shifted = calloc(BUFFER_SIZE, sizeof(float));
    if(shift > 0)
    {
        for(int i=shift;i<BUFFER_SIZE;i++)
            x_in_shifted[i] = x_in[i-shift];
    }
    else if(shift <= 0)
    {
        for(int i=0;i<BUFFER_SIZE+shift;i++)
            x_in_shifted[i] = x_in[i-shift];
    }
    for(int i = 0;i < BUFFER_SIZE; i++)
    {
        cor += x_out[i]*x_in_shifted[i];
        norm_out += x_out[i]*x_out[i];
        norm_in += x_in_shifted[i]*x_in_shifted[i];
    }
    cor /= sqrt((norm_out*norm_in));
    return cor;
}

int find_shift()
{
    int shift = -2*buff_per_spin;
    double cor = 0.0;
    int best_shift = 0;
    for(;shift < 2*buff_per_spin+1; shift++)
    {
        if(correlation(shift) > cor)
            best_shift = shift;
    }
    return best_shift;
}


void single_iteration(float alpha, float beta, int s,int iteration)
{
    int i, n;

    //compute x_out

    // Multiiply by alpha aOne waveform per burstnd add noise
    float *feedback_terms = (float *)calloc(N_spins, sizeof(float)); 

    feedback(feedback_terms, alpha, beta);
    // Calculate the feedback value according to the equation
    for(i = 0;i < N_spins;i++)
    {
        n = rand()%N_noise;
        float value = -feedback_terms[i] + noise[n];
        //printf("feedback_terms[%d] = %f noise = %f value = %f\n",i,feedback_terms[i],noise[n],value); 
	    //printf("Value after cos is: %f \n",value);

        //Threshold the output
        if(value >= 1.0)
        {
            value = 1.0;
        }
        else if(value<=-1.0)
        {
            value = -1.0;
        }
        
	    // Remove in lab
        value = pow(cos(value + (0.25*M_PI)),2); //Modulator function. 	 
	    //printf("Value = %f\n", value);
        //value = 0.01*n;
	
        // x_out an array that will store the output
    
        // Store the value in the buffer to be given as output for the next
        // buff_size cycles
        for(int j = i*buff_per_spin;j < (i+1)*(buff_per_spin);j++)
        {
            x_out[j] = value;
        }
    }
    
    // Fill DAC buffer
    rp_GenArbWaveform(RP_CH_2, x_out, buff_size); 
    
    //Reset Acquisition to Defaults
    rp_AcqReset();
    

    // Configure acquisition
    rp_AcqSetTriggerDelay(trig_delay);
    
    //Start Acquisition
    rp_AcqStart();

    //Start DAC Operation
    rp_GenOutEnable(RP_CH_2);

    // Trigger immediately
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_NOW);
    

    rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;    

    i = 0;
    // Wait for buffer to get full after Trigger.
    do
    {
	    rp_AcqGetTriggerState(&state);
	}while(state == RP_TRIG_STATE_TRIGGERED);
	
    //Get data from buffer to code
    rp_AcqGetOldestDataV(RP_CH_1, &buff_size, x_in);

    // Stop acquisition
    rp_AcqStop();
    
    //Disable output
    rp_GenOutDisable(RP_CH_2);

    //Set new trigger delay
    trig_delay = t2;


    for(i = 0;i < N_spins; i++)
    {
	    int index = (2*i+1)*buff_per_spin/2;
        x_k[i] = (scale*x_in[index]/att)-offset;
    }

    fprintf(fp2,"%f %f %d %d",alpha,beta,s,iteration);

    for(i=0;i<buff_size;i++)
    {
	fprintf(fp,"iter=%d %d %f %f\n",iteration,i,x_out[i],x_in[i]/att);
    }
    for(i = 0;i < N_spins; i++)
    {
        fprintf(fp2," %f",x_k[i]);
    }
    fprintf(fp2,"\n");

}

float cut_value()
{
	float value = 0;
	for(int i=0;i<N_spins;i++)
	{
		for(int j=0;j<N_spins;j++)
		{
			if(x_k[i]*x_k[j]<0)
			{
				value = value + J[i][j];
			}
		}
	}
	value/=2;
	return value;
}

int main (int argc, char **argv) 
{
    system ("cat /opt/redpitaya/fpga/fpga_0.94.bit > /dev/xdevcfg");
    struct tm *timenow;
    char sync_filename[60], traj_filename[60], cut_filename[60];

    time_t now = time(NULL);
    timenow = localtime(&now);
    strftime(sync_filename, sizeof(sync_filename), "./data/xout_xin_%d_%m_%Y_%H_%M_%S.csv", timenow);
    strftime(traj_filename, sizeof(traj_filename), "./data/trajectory_%d_%m_%Y_%H_%M_%S.csv", timenow);
    strftime(cut_filename, sizeof(cut_filename), "./data/cut_%d_%m_%Y_%H_%M_%S.csv", timenow);
 
    char comment[150];
    printf("Enter comment on file: ");
    fgets(comment, sizeof(comment), stdin);  // read string

    fp = fopen(sync_filename,"w");
    fp2 = fopen(traj_filename,"w");
    fp3 = fopen(cut_filename,"w");
    j_file = fopen("./Maxcut_instances/J_3spins.txt", "r");
    
    fprintf(fp, "#%s\n", comment);
    fprintf(fp2, "#%s\n", comment);
    fprintf(fp3, "#%s\n", comment);
    fprintf(fp3, "#Alpha Beta Cut\n");
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
    noise = (float *)calloc(N_noise, sizeof(float));
    x_k = (float *)calloc(N_spins, sizeof(float));
    
    int i,s;

    J = (float **)calloc(N_spins, sizeof(float *));
    for(int k = 0;k < N_spins;k++)
    {
        J[k] = (float *)calloc(N_spins, sizeof(float));
    }

    read_J();

    
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
 
    //Configure ADC
    rp_AcqReset();
    rp_AcqSetDecimation(1);

    //Configure DAC
    
    //Set arbitrary waveform
    rp_GenWaveform(RP_CH_2, RP_WAVEFORM_ARBITRARY);
    
    // Enable burst mode
    rp_GenMode(RP_CH_2, RP_GEN_MODE_BURST); 
    
    // set burst count
    rp_GenBurstCount(RP_CH_2, bcounts);
   
     // set burst repetitions
    rp_GenBurstRepetitions(RP_CH_2, breps);
   
    //Set Generation Frequency
    rp_GenFreq(RP_CH_2, freq);
    
    
    //Adjust this later
    buff_per_spin = (int)BUFFER_SIZE/N_spins;

    fprintf(fp2,"#Alpha Beta Run/Spin Iteration Values\n");
   
    for(alpha = ALPHA_MIN;alpha <= ALPHA_MAX;alpha += ALPHA_STEP)
    {
        for(beta = BETA_MIN;beta <= BETA_MAX;beta += BETA_STEP)
        {
            // Generate new noise for each alpha
            gen_noise();

            for(s = 0;s < N_runs; s++)
            {
                for(i = 0;i < N_iters;i++)
                {
                    single_iteration(alpha, beta, s, i);
                }
		        float cut_store = cut_value();
	            printf("Alpha = %f Beta = %f The cut value is: %f\n",alpha,beta, cut_store);
		        fprintf(fp3,"%f %f %f\n",alpha,beta,cut_store);
                free(x_k);
                x_k = (float *)calloc(N_spins, sizeof(float));
            }
        }
    }

    // Releasing resources
    free(x_out);
    free(x_in);
    free(noise);
    rp_Release();
    fclose(fp);
    fclose(fp2);
    fclose(fp3);
    return EXIT_SUCCESS;
}
