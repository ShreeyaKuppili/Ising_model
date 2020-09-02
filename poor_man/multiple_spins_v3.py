import numpy as np
import matplotlib.pyplot as plt
import sys


N = 3           # Number of spins
N_iters = 100   # Number of iterations per run
N_runs = 10     # Number of runs 

max_alpha = 4.0
min_alpha = -4.0
alpha_step = 0.1
plot_alpha = (max_alpha + min_alpha)/2

max_beta = 2.0
min_beta = 0.1
beta_step = 0.1
plot_beta = (max_beta + min_beta)/2

sig = 0.04          # Std Deviation of noise
offset = 0.5    
V_pi = 1.0          # V_pi of the modulator
I0 = 1.0            # Action of photodiode and MZM
DC_bias = V_pi/4    # Action of DC_bias

J_file = 0          #Flags which enable and disable different features
bifurcation = 0 
trajectory = 0
solver = 0
data_file = 0
tanh_switch = 1

#print("sys.argv is :",sys.argv)
if(len(sys.argv)>1):
    i = 1
    while(i <= len(sys.argv[1:])):
        opt = str(sys.argv[i])
        #print(opt)

        #Alpha parameters
        if(opt == '-aplt'):
            plot_alpha = float(sys.argv[i+1])
        elif(opt == '-ua'):
            max_alpha = float(sys.argv[i+1])
        elif(opt == '-da'):
            min_alpha = float(sys.argv[i+1])
        elif(opt == '-sa'):
            alpha_step = float(sys.argv[i+1])
        elif(opt == '-a'):
            min_alpha = float(sys.argv[i+1])
            max_alpha = min_alpha + 1e-10
            plot_alpha = min_alpha
        
        #Beta parameters
        elif(opt == '-bplt'):
            plot_beta = float(sys.argv[i+1])
        elif(opt == '-ub'):
            max_beta = float(sys.argv[i+1])
        elif(opt == '-db'):
            min_beta = float(sys.argv[i+1])
        elif(opt == '-sb'):
            beta_step = float(sys.argv[i+1])
        elif(opt == '-b'):
            min_beta = float(sys.argv[i+1])
            max_beta = beta + 1e-10
            plot_beta = min_beta

       #Run parameters
        elif(opt == '-iter'):
            N_iters = int(sys.argv[i+1])
        elif(opt == '-N'):
            N = int(sys.argv[i+1])
        elif(opt == '-run'):
            N_runs = int(sys.argv[i+1])

        #Modulator parameters
        elif(opt == '-dc'):
            DC_bias = float(sys.argv[i+1])
        elif(opt == '-o'):
            offset = float(sys.argv[i+1])
        elif(opt == '-I'):
            I0 = float(sys.argv[i+1])
        elif(opt == '-pi'):
            V_pi = float(sys.argv[i+1])
        
        #Noise parameters
        elif(opt == '-sig'):
            sig = float(sys.argv[i+1])
        
        #Config parameters        
        elif(opt == '-J'):
            J_file = str(sys.argv[i+1])
        elif(opt == '-sol'):
            solver = 1
            #i+=1
        elif(opt == '-traj'):
            trajectory = 1
            #i+=1
        elif(opt == '-bif'):
            bifurcation = 1
            #i+=1
        elif(opt == '-data'):
            data_file = str(sys.argv[i+1])
        elif(opt == '-notanh'):
            tanh_switch = 0
        #sys.argv.pop(i+1)
        i+=1



def modulator(x):
    return I0*pow(np.cos(x/V_pi + DC_bias*np.pi/V_pi),2) #Check this equation later

def feedback(x,alpha,beta,J):
    J = beta*J
    np.fill_diagonal(J,-alpha)
    return J@x

def notanh(x):
    return np.array([1 if i>=1 else -1 if i<=-1 else i for i in x])

def cut_value(cut):
    cut_value = 0
    for row,i in enumerate(cut):
        for col,j in enumerate(cut):
            if(i*j<0):
                #print(i,j)
                #print("Row and column",row,col,J[row][col]/beta)
                cut_value = cut_value + J[row][col]
    return cut_value



#Initialize J to a ring if there is no input
J = np.zeros([N,N])
for i in range(1,N-1):
    J[i][i+1] = 1
    J[i][i-1] = 1
J[0][1] = 1
J[0][N-1] = 1
J[N-1][N-2] = 1
J[N-1][0] = 1

#Load J from a file
if(J_file):
    f = open(J_file,"r")
    J = np.zeros([N,N])
    for i,line in enumerate(f.readlines()):
        J[i,:] = np.array([float(i) for i in line.split()])
    #print(J.shape)
    #print(np.where(J == J.T)[0].shape)
    f.close()

#Create Alpha and Beta arrays
Alpha = np.arange(min_alpha,max_alpha,alpha_step)
Beta = np.arange(min_beta,max_beta,beta_step)
switch = 0
print(Alpha)
exit()
final_x = np.zeros([N,1])
pre_final_x = np.zeros([N,1])
traj_x = np.zeros([N,1])

solutions = []

for alpha in Alpha:
    for beta in Beta:
        for run in range(N_runs):
            x_k = np.zeros([N,1])
            x_in = np.zeros([N,1])
            #print("Working with Alpha = {}, Beta = {}".format(alpha,beta))
            i = 0
            traj_x = x_in
            noise = np.random.normal(0,sig,[N,N_iters])
            while(i < N_iters):
                # This is the calculated value to be put out to the DAC
                # Put the coupled equation instead later
                x_out = 2*np.around((feedback(x_k, alpha,beta,J) + np.array([noise[:,i]]).T)/2,3)

                # The value received from the modulator
                if(tanh_switch):
                    x_in = 2*np.around(modulator(np.tanh(x_out))/2,3)
                else:
                    x_in = 2*np.around(modulator(notanh(x_out))/2,3)

                # The state value
                x_k = x_in-offset

                # print("x_k = ",x_k)
                i += 1
                # Add to trajectory
                traj_x = np.c_[traj_x,x_k]
            
            if(plot_alpha<=alpha and switch == 0 and plot_beta<=beta):
                # plot trajectory
                if(trajectory):
                    switch = 1
                    plot1 = plt.figure(1)
                    for i in range(N):
                        plt.plot(traj_x[i],"-",label = "spin {}".format(i))
                    #plt.legend()       
                    plt.title("Run number = {} Alpha = {} Beta = {}".format(run,alpha,beta))
        
            pre_final_x = np.c_[pre_final_x,traj_x[:,-2]]
            final_x = np.c_[final_x,traj_x[:,-1]]
            cut = np.sign(traj_x[:,-1])
            if(solver):
                solutions.append([alpha,beta,run,cut_value(cut),cut])




if(bifurcation):
    plot2 = plt.figure(2)
    plt.plot(Alpha,final_x[:,1::N_runs*len(Beta[:np.where(plot_beta<Beta)[0][0]])].T,"r.",markersize=1)
    plt.title("Final value vs Alpha")
    plot3 = plt.figure(3)
    plt.plot(Beta,final_x[:,1::N_runs*len(Beta)].T,"r.",markersize=1)
    # # plt.plot(Alpha,pre_final_x[:,1:].T,"r.",markersize=0.5)

if(solver):
    opt_cut_value = 0
    opt_alpha = 0
    opt_beta = 0
    opt_cut = np.zeros(N)
    for i in solutions:
        if (i[3] > opt_cut_value):
            opt_cut_value = i[3]
            opt_alpha = i[0]
            opt_beta = i[2]
            opt_cut = i[4]
    print("The nodes on two sides of the cut are: ")
    print("The negative side:")
    for node,i in enumerate(opt_cut):
        if(i<0):
            print(node+1)
    print("The positive side:")
    for node,i in enumerate(opt_cut):
        if(i>0):
            print(node+1)
    #print(cut)
    print("The number of positive nodes is: ",len(np.where(opt_cut>0)[0]))
    print("The number of negative nodes is: ",len(np.where(opt_cut<0)[0]))

    print("The best cut value that was obtained is: ")
    print("Cut value: {}\nAlpha: {}\nBeta: {}".format(opt_cut_value,opt_alpha,opt_beta))

plt.show()
