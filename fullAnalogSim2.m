clc; clear all; close all;
%%%%%%%%%%system variables%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
xdim = 4;                           %subplot variables
ydim = 6;                           %subplot variables
counter=5;                          %subplot variables
simTime = 10E-3;                    %length of simulation in seconds
Fs = 100E6;                         %sampling frequency for simulation
Fsam2 = 110E3;                      %second sampling frequency
Tsam2 = 1/Fsam2;                    %Time step for second sampling frequncy
T = 1/Fs;                           %time step
Fundersample=120E3;
%fundersample = 0:1E3:100E3;
Tundersample=1/Fundersample;
tundersample = 0:Tundersample:simTime;
%n = 2^nextpow2(Fs);
t = 0:T:simTime;                    %time vector
N = round(simTime/T);               %vector length
n = 2^nextpow2(simTime/T);          %vector length (power of 2 for FFT)
fstep = 1/simTime;                  %step of frequency for spectra 
f = 0:fstep:(Fs/2);                 %frequency vector
f2 = 0:fstep:(Fsam2/2)-1;

figure(1);
set(gcf, 'units','normalized','outerposition',[0 0 1 1]);
%%%%%%%%%signal variables%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%

%fvariables = 50E3*randn(10)+50E3;
fvariables = [8300, 26E3, 30.5E3, 32E3, 35E3, 75E3, 86E3, 88E3, 89.5E3, 95E3];
vin = zeros(1, N+1);
for i=1:10
    vin = vin + 5*cos(2*pi*fvariables(i)*t);
end
%vin= 5*(1*cos(2*pi*fin*t)+0*cos(2*pi*(fin+10E3)*t)+cos(2*pi*(fin+85E3)*t));%+cos(2*pi*(fin-3E3)*t));        %input signal
%vin = cos(2*pi*60E3*t);

for i=1:10
    %%%%%%%%%local oscillator 1%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    fin = 10E3*i-10E3;

    fLO1 =(240E3-fin);                      %local oscillator frequency
    LO1 = cos(2*pi*fLO1*t);               %local oscillator

    %%%%%%%%first multiplication stage%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    mux1 = LO1 .* vin;

    %%%%%%%%band pass filter%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    filt1 = zeros(1, N);
    filt2 = zeros(1, N);
    filt3 = zeros(1, N);

    %RC stage 1 values
    R1 = 158E3;
    R2 = 133;
    R3 = 316E3;
    C1 = 100E-12;
    C2 = 100E-12;
    %BPF stage 1 constants
    A1 = -1/(R1*C1);
    B1 = 1/(R3*C2) + 1/(R3*C1);
    G1 = (1/(R3*C1*C2))*(1/R1 + 1/R2);
    D1 = 2-T*B1;
    E1 = -1 + T*B1 - G1*T^2;
    F1 = A1*T;
    for k=2:N-1
        filt1(k+1) = D1*filt1(k) + E1*filt1(k-1) + F1*mux1(k) - F1*mux1(k-1);
    end
    %RC stage 2 values
    R1 = 162E3;
    R2 = 68.1;
    R3 = 649E3;
    C1 = 100E-12;
    C2 = 100E-12;
    %BPF stage 2constants
    A1 = -1/(R1*C1);
    B1 = 1/(R3*C2) + 1/(R3*C1);
    G1 = (1/(R3*C1*C2))*(1/R1 + 1/R2);
    D1 = 2-T*B1;
    E1 = -1 + T*B1 - G1*T^2;
    F1 = A1*T;
    for k=2:N-1
        filt2(k+1) = D1*filt2(k) + E1*filt2(k-1) + F1*filt1(k) - F1*filt1(k-1);
    end
    %RC stage 3 values
    R1 = 154E3;
    R2 = 64.9;
    R3 = 619E3;
    C1 = 100E-12;
    C2 = 100E-12;
    %BPF stage 3 constants
    A1 = -1/(R1*C1);
    B1 = 1/(R3*C2) + 1/(R3*C1);
    G1 = (1/(R3*C1*C2))*(1/R1 + 1/R2);
    D1 = 2-T*B1;
    E1 = -1 + T*B1 - G1*T^2;
    F1 = A1*T;
    for k=2:N-1
        filt3(k+1) = D1*filt3(k) + E1*filt3(k-1) + F1*filt2(k) - F1*filt2(k-1);
    end


    %%%%%LO2%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    %fLO2 = 230E3; 
    fLO2 = 260E3;
    LO2 = cos(2*pi*fLO2*t);     %cut off last value;
    mux2 = zeros(1,N);
    mux2 = filt3 .* LO2(1:N);        

    %%%%%LPF%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    filt4 = zeros(1, N);
    Rlpf = 1.32E3;
    Clpf = 1E-9;
    H = T/(Rlpf*Clpf);
    for k=1:N
        filt4(k+1) = filt4(k)*(1-H) + mux2(k)*H;
    end

    %%%%%%%%%%%%%decimation for A/D simulation%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    decimation = round(Fs/Fsam2);
    dec = zeros(1, round(N/decimation));
    for n=decimation:decimation:N                             %10:1 Decimation
        dec(round(n/decimation))=filt4(n);
    end
    N2 = round(simTime/(Tsam2));

    %fft system with LO2 (low speed ADC)
    Q = fft(dec);
    P8 = abs(Q/2);
    spectraLO2lowSpeed = P8(1:(N2)/2);
    spectraLO2lowSpeed(2:end-1) = 2*spectraLO2lowSpeed(2:end-1);

    fnow = f2+fin-5E3+fstep;
    spectraLO2lowSpeedflip = fliplr(spectraLO2lowSpeed);
    %spectraLO2lowSpeedflip = spectraLO2lowSpeed;
    
    subplot(ydim, xdim, counter);
    %plot(fnow(50:150), spectraLO2lowSpeedflip(50:150));
    plot(fnow(50:150), fliplr(spectraLO2lowSpeed(100:200)));
    %plot(spectraLO2lowSpeed);
    str = sprintf('LO2, LO1 @ %d Hz', fLO1);
    title(str);
    ylim([0, 1000]);
    counter = counter + 1;
    
    %%%undersample%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    Vundersample = zeros(1, N+1);
    a=1;
    A = Tundersample/T;
    %s/h
    for m=1:round(N/A)
        Vsam = filt3(a);
        for p=1:(A)-1
            Vundersample(a) = Vsam;
            a=a+1;
        end
    end
    Y = fft(Vundersample);
    P1 = abs(Y/2);
    spectraUndersample = P1(1:(N)/2);
    spectraUndersample(2:end-1) = 2*spectraUndersample(2:end-1);
    fundersample = fin+100:1E2:(fin+10E3+100);
    subplot(ydim, xdim, counter);
    plot(fundersample(1:100), spectraUndersample(1:100));
    str = sprintf('Undersample, LO1 @ %d Hz', fLO1);
    title(str);
    %xlim([0, 100]);
    ylim([0, 2E6]);
    counter = counter + 1;


    %%s/h%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
    

end
%fft input
Z = fft(vin);
P4 = abs(Z/(N));
spectraInput = P4(1:round(((N)+1)/2));
spectraInput(2:end-1) = 2*spectraInput(2:end-1);
subplot(ydim, xdim, [1:4]);
plot(f(1:1000), spectraInput(1:1000));
title('Spectra of Input (high speed samping) [Zoomed]');
counter = counter + 1;

%figure(); plot(spectraLO2lowSpeed);

K = abs(fft(mux2)); figure; plot(K(1:300));
%figure; plot(M(2300:3E3));

