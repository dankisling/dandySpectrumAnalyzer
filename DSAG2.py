# This code was developed by Dan Kisling and Andrew Shapiro as part of a senior Design Project for Lehigh University
# Large thanks to pythonprogramming.net for help useful examples used to make this program.
# Last updated 5/04/16 by Dan Kisling

import matplotlib           #used for graphing
import subprocess           #used to run c program
import math
import time                 #used for sleep function 
matplotlib.use("TkAgg")     #backend for Tkinter
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2TkAgg
from matplotlib.figure import Figure
import matplotlib.animation as animation
from matplotlib import style
from subprocess import call     #used to instantiate driver and run driver

import tkinter as tk
from tkinter import ttk
from matplotlib import pyplot as plt

LARGE_FONT= ("Verdana", 12)
NORM_FONT= ("Verdana", 10)
SMALL_FONT= ("Verdana", 8)

style.use("ggplot")

f = Figure(figsize=(1,1))
a = f.add_subplot(111)

getData = True                  #used for troubleshooting. False turns off all subprocess commands throughtout application

# Default Settings
chartLoad = True                #play / pause for display and data acquisition
chartLoadstr = "Pause"          #string for menu change purposes
peakFrequencyShow = False       #boolean. When true annotate graph to show peak frequency values
energyShow = False              #boolean. When true annotate graph to show sum of energy across bandwidth
AenergyShow = False             #boolean. When true annotate graph to show average of energy across bandwidth
aP = True                       #boolean. Necessary so pause written exactly once to screen when appropriate
gain = '1'                      #gain setting to be sent to hardware
flow = 0                        #Lower bound of frequency for energy measurements
fhigh = 1                       #Upper bound of frequency for energy measurements
xscale = 'linear'                  #Sets x-axis scale: either 'linear' or 'log'
yscale = 'linear'               #Sets y-axis scale: either 'linear' or 'log'
clippingOverride = False        #Overrides warning on screen when clipping is believed to occur
clippingWarningMenuMessage = "Turn Off Clipping Warning Notification"   #string for menu change purposes
plotColor = '#183A54'           #Hex value for color of plot
currentView = 'frequency'       #what domain output should be in
currentViewMsg = "Switch to Time Domain View"   #string for menu change purposes
altDesign = False               #Boolean for aquisition method. False = full system. True = alternate design
LOvalue = 360000                #Default value for LO (only used in AltDesign)
correction = -1945              #Default value for correction (only used in AltDesign)

def runCapture():               #this function runs the driver that physically inputs the values into the Pi
    global gain
    global altDesign
    global LOvalue
    global correction
    
    bit0 = '0'
    bit1 = '0'
    bit2 = '1'

    if(gain=='0'):
        bit0 = '0'
        bit1 = '0'
        bit2 = '0'
    elif(gain=='1'):
        bit0 = '1'
        bit1 = '0'
        bit2 = '0'        
    elif(gain=='2'):
        bit0 = '0'
        bit1 = '1'
        bit2 = '0'
    elif(gain=='5'):
        bit0 = '1'
        bit1 = '1'
        bit2 = '0'
    elif(gain=='10'):
        bit0 = '0'
        bit1 = '0'
        bit2 = '1'
    elif(gain=='20'):
        bit0 = '1'
        bit1 = '0'
        bit2 = '1'
    elif(gain=='50'):
        bit0 = '0'
        bit1 = '1'
        bit2 = '1'
    elif(gain=='100'):
        bit0 = '1'
        bit1 = '1'
        bit2 = '1'

    gainCommand = "-a="+bit2+bit1+bit0
    oscCom = "-l="+str(LOvalue)
    corCom = "-c="+str(correction)
        
    if(getData):
        if(altDesign):
            subprocess.call(['sudo', './specAn_controller', '-s=1', gainCommand, corCom, oscCom, '-o'])
        else:
            subprocess.call(['sudo', './specAn_controller', '-s=20', gainCommand, corCom, '-l=355000,360000,365000,370000,375000,380000,385000,390000,395000,400000,405000,410000,415000,420000,425000,430000,435000,440000,445000,450000'])
           
def about():                    #this function handles the about button in the help menu
    def page2():
        abt.destroy()
    abt = tk.Tk()
    abt.wm_title("About")
    label = ttk.Label(abt, text="""
                       Dandy Spectrum Analyzer was developed by Dan Kisling and Andrew Shapiro
                      as part of a senior design project for Lehigh University under Advisor Professor Frey
                      It uses a Raspberry Pi as the brain of a Spectrum Analyzer. More info can be found at
                      www.dankisling.wordpress.com""", font=NORM_FONT)
    label.pack(side="top", fill="x", pady=10)
    B1 = ttk.Button(abt, text = "K. Cool", command=page2)
    B1.pack()
    

def tutorial():                 #this function handles the tutorial button in the help menu
    def page2():
        tut.destroy()
        tut2 = tk.Tk()
        def page3():
            tut2.destroy()
            tut3 = tk.Tk()
            tut3.wm_title("Part 3!")
            label = ttk.Label(tut3, text="Part 3", font=NORM_FONT)
            label.pack(side="top", fill="x", pady=10)
            B1 = ttk.Button(tut3, text="Done!", command= tut3.destroy)
            B1.pack()
            tut3.mainloop()
        tut2.wm_title("Part 2!")
        label = ttk.Label(tut2, text="Part 2", font=NORM_FONT)
        label.pack(side="top", fill="x", pady=10)
        B1 = ttk.Button(tut2, text="Next", command= page3)
        B1.pack()
        tut2.mainloop()
    tut = tk.Tk()
    tut.wm_title("Tutorial")
    label = ttk.Label(tut, text="What do you need help with?", font=NORM_FONT)
    label.pack(side="top", fill="x", pady=10)
    B1 = ttk.Button(tut, text = "Overview of the application", command=page2)
    B1.pack()
    B2 = ttk.Button(tut, text = "That's not the Spectra I expected. Help!", command=lambda:popupmsg("Not yet completed"))
    B2.pack()
    B3 = ttk.Button(tut, text = "Cancel", command=lambda:popupmsg("Well okay then."))
    B3.pack()
    tut.mainloop()

def changeGain(howMuch):        #this function changes the gain of the input amplifier in the hardware
    global gain
    gain = howMuch
    

    
    print("changed gain to: "+howMuch) 
    
def measure(what):              #this function polls the user on specifications of how they want their measurements to take place
    global peakFrequencyShow
    global flow
    global fhigh
    global energyShow
    global AenergyShow
    
    if what=="f":
        peakFrequencyShow = ~peakFrequencyShow
    if (what=="e" or what=="a"):
        if(energyShow or AenergyShow):
            energyShow = False #toggle display
            AenergyShow = False
        else:
            if(what=="e"):
                energyShow = True
            else:
                AenergyShow = True
            sett = tk.Tk()
            sett.wm_title("Set frequency Range")
            label = ttk.Label(sett, text="Choose Frequency Range")
            label.pack(side="top", fill="x", pady=10)
            e1 = ttk.Entry(sett)
            e1.insert(0, 20)
            e1.pack()
            e1.focus_set()
            label = ttk.Label(sett, text="to")
            label.pack()
            e2 = ttk.Entry(sett)
            e2.insert(1, 200)
            e2.pack()
            e2.focus_set()

            def callback():
                global flow
                global fhigh
                flow = (e1.get())
                fhigh = (e2.get())
                print("low f:"+str(flow)+"\nhigh f:"+str(fhigh))
                sett.destroy()
            b = ttk.Button(sett, text="Submit", width=10, command=callback)
            b.pack()
            tk.mainloop()

def disp(what):                     #This function changes display settings
    global xscale
    global yscale
    global clippingOverride
    global clippingWarningMenuMessage
    global plotColor
    global currentView
    global currentViewMsg

    if(what=='t'):                  #change output domain
        if(currentView=='time'):    #was time
            currentView='frequency' #now frequency
            currentViewMsg = "Switch to Time Domain View" #update menu option
        else:                       #was frequency
            currentView = 'time'    #now time
            currentViewMsg = "Switch to Frequency Domain View"#update menu option
        print("view changed to "+currentView+" domain")
    if(what=='x'):                  #change x-axis scale
        if(xscale=='linear'):       #was linear
            xscale='log'            #now log
        else:
            xscale='linear'         #was log, now linear
        print("x scale now "+xscale)
    elif(what=='y'):                #change y-axis scale
        if(yscale=='linear'):       #was linear
            yscale='log'            #now log
        else:
            yscale='linear'         #was log, now linear
        print("y scale now "+yscale)
    elif(what=='c'):                #change color
        sett = tk.Tk()              #poll user through popup message
        sett.wm_title("Set plot color")
        label = ttk.Label(sett, text="Input HEX color name")
        label.pack(side="top", fill="x", pady=10)
        e1 = ttk.Entry(sett)
        e1.insert(0, plotColor)     #show user default as previous color value
        e1.pack()
        e1.focus_set()
        def callback():
            global plotColor
            plotColor = (e1.get())  #note no error detection
            print("color set to: "+plotColor)
            sett.destroy()
        b = ttk.Button(sett, text="Set", width=10, command=callback)
        b.pack()
        tk.mainloop()
    elif(what=='co'):               #change clipping override settings
        if(clippingOverride):       #was on
            clippingOverride = False#turn off
            clippingWarningMenuMessage = "Turn Off Clipping Warning Notification"
        else:
            clippingOverride = True #was off, turn on
            clippingWarningMenuMessage = "Turn On Clipping Warning Notification"
        print("clippingOverride = "+str(clippingOverride))

    
def loadChart():                #This funciton starts and stops the data aquisition and display 
    global chartLoad
    global chartLoadstr
           
    if(chartLoad):              #was running
        chartLoad = False       #now stop
        print("stopped")        #print stopped
        chartLoadstr = "Start"  #change menu button to Start
    else:                       #was stopped
        chartLoad = True        #now run
        print("started")        #print started
        chartLoadstr = "Stop"   #change menu button to Stop

def changeAcq(pn):      #This function changes the acquisition method
    global altDesign
    global LOvalue
    global correction

    if(pn=='m'):    #change aquisition method
        if(altDesign):
            altDesign = False
            popupmsg("Flip Aquisition Switch to Full")
        else:
            altDesign = True
            popupmsg("Flip Aquisition Switch to Alt")
    elif(pn=="o"):  #change local oscillator value
        if(altDesign):
            sett = tk.Tk()              #poll user through popup message
            sett.wm_title("Set Oscillator Frequency")
            label = ttk.Label(sett, text="Input Oscillator Frequency (Hz)")
            label.pack(side="top", fill="x", pady=10)
            e1 = ttk.Entry(sett)
            e1.insert(0, LOvalue)     #show user default as previous oscillator value
            e1.pack()
            e1.focus_set()
            def callback():
                global LOvalue
                LOvalue = (e1.get())  #note no error detection
                print("Oscillator set to: "+LOvalue)
                sett.destroy()
            b = ttk.Button(sett, text="Set", width=10, command=callback)
            b.pack()
            tk.mainloop()
        else:
            popupmsg("This functionality only works in AltDesign Mode")
    elif(pn=="c"):  #change correction
        if(altDesign):
            sett = tk.Tk()              #poll user through popup message
            sett.wm_title("Set Correction Frequency")
            label = ttk.Label(sett, text="Input Correction Frequency (Hz)")
            label.pack(side="top", fill="x", pady=10)
            e1 = ttk.Entry(sett)
            e1.insert(0, correction)     #show user default as previous correction value
            e1.pack()
            e1.focus_set()
            def callback():
                global correction
                correction = (e1.get())  #note no error detection
                print("Correction set to: "+correction)
                sett.destroy()
            b = ttk.Button(sett, text="Set", width=10, command=callback)
            b.pack()
            tk.mainloop()
        else:
            popupmsg("This functionality only works in AltDesign Mode")
            
    else:
        pass

def popupmsg(msg):              #This function allows us to create popup messages
    popup = tk.Tk()

    def leavemini():
        popup.destroy()
    
    popup.wm_title("!")
    label = ttk.Label(popup, text=msg, font=NORM_FONT)
    label.pack(side="top", fill="x", pady=10)
    B1 = ttk.Button(popup, text="Okay", command = leavemini)
    B1.pack()
    popup.mainloop()


def animate(i):                 #This function is the heart of the program. It runs every INTERVAL milliseconds (can change at bottom of code)
    global chartLoad
    global peakFrequencyShow
    global energyShow
    global flow
    global fhigh
    global AenergyShow
    global aP
    global xscale
    global yscale
    global clippingOverride
    global plotColor
    global currentView
    global gain
    global altDesign
    
    if(chartLoad):              #if we are running, run data acquisition and display
        runCapture()            #run data acquisition
        if(currentView=='frequency'):          
            pullData = open("controller.log","r").read()   #open data file for read in
            dataList = pullData.split('\n')         #split each line
            xList = []      #list for all x values to plot
            yList = []      #list for all y values to plot
            index = 0       #holds frequency value in which ymax occurs
            yE = 0          #accumulator for energy in bandwidth specified
            yNE = 0         #accumulator for energy not in the bandwidth specified
            ymax = -60        #highest y value that data achieves
            inc = 0         #counter to step through data file 
            tdPeak = 0      #time domain peak voltage level code
            for eachLine in dataList:               #step through each line of the data file
                if len(eachLine) > 1:               #that is, if there is data on that line
                    if(altDesign):
                        if(inc>4096):                   #if we've read half of the values, our work is done
                            print(str(inc))
                            break                       #stop reading values
                    xs, ys = eachLine.split(' ')    #split each line (by space inbetween numbers)
                    x = float(xs)                   #left value is x (cast to float)
                    if(altDesign):
                        y = float(ys) * 201.2 * .3535 / float(gain)
                    else:
                        y = float(ys) * 3525 * .3535 / float(gain)
                    #y = 10*math.log(float(ys),10)                  #right value is y (cast to float)
                    if(inc==0):                     #ignore the DC value 
                        yList.append(float(0))
                        tdPeak = y                  #the first y value is actually the time domain peak value
                    else:
                        yList.append(y)             #add next y value to list
                        if(y>ymax):                 #check to see if this is the biggest y we've seen so far
                            ymax = y                #if so, record it
                            index = x               #save that x value as well
                    xList.append(x)                 #add next x value to list
                    inc= inc + 1
    ##                if(int(x)>=int(flow) and int(x)<=int(fhigh)):
    ##                    yE = yE + y
    ##                else:
    ##                    yNE = yNE + y        
            a.clear()                               #clear previous plot
            a.plot(xList, yList, color=plotColor)   #plot new values
            title = 'Spectra of Input'              #title of our plot
            a.set_xlabel('Frequency (Hz)')
            a.set_ylabel('Magnitude (VRMS)')
            a.set_title(title)                      #Display title
            a.set_yscale(yscale)                    #set y scale 
            a.set_xscale(xscale)                    #set x scale

            if(tdPeak > 132):                       #warn our user if we believe clipping is occuring
                if(clippingOverride==False):
                    a.text(100000, ymax, "Clipping Warning!", fontsize=12)
            if(peakFrequencyShow):                  #show peak frequency if option is chosen
                a.annotate('Frequency:'+str(index)+' Hz\nLevel:'+str(ymax)+' Vrms', xy=(index, ymax), xytext=(100, ymax),
                           arrowprops=dict(facecolor='black', shrink=0.05))
            if(energyShow):                         #show energy if that option is chosen
                energy = yE 
                Nenergy = yNE 
                a.text(fhigh, ymax/3, "Energy in BW:"+str(energy)+
                       "\nEnergy not in BW:"+str(Nenergy), fontsize=12)
            if(AenergyShow):                        #show average energy if that option is chosen
                energy = yE / (int(fhigh) - int(flow))
                Nenergy = yNE / (int(x) - (int(fhigh) - int(flow)))
                a.text(fhigh, ymax/3, "Average Energy in BW:"+str(energy)+
                       "\nAverage Energy not in BW:"+str(Nenergy), fontsize=12)
            aP = True                               #set flag that pause has not been displayed
        else:           #time domain capture
            pullData = open("time.log","r").read()  #open data file for read in
            dataList = pullData.split('\n')         #split each line
            tList = []      #list for discrete time points
            yList = []      #list for amplitudes of discrete time points
            inc = 0         #increment through each loop in for loop below
            for eachLine in dataList:           #step thorugh each line of the data file
                if len(eachLine) > 1:           #that is, if there is data on that line
                    if(inc>8192):               #stop reading values if we have read more than 8192 values
                        break
                ts, ys = eachLine.split(' ')    #split each line (by space inbetween numbers0
                t = float(ts)                   #left value is time
                y = float(ys)                   #right value is amplitude
                yList.append(y)                 #append the amplitude to the amplitude list
                tList.append(t)                 #append the time to the time list
                inc = inc +1                    #increment to show we have finished a loop
            a.clear()                               #clear previous plot
            a.plot(tList, yList, color=plotColor)   #plot new values
            title = 'Output'                        #title of our plot
            a.set_title(title)                      #display title
            a.set_xscale('linear')                  #set xscale to linear (log doesn't make sense)
            a.set_yscale('linear')                  #set yscale to linear (log doesn't make sense)
            aP = True                           #set flag that pause has not been displayed
    else:
        if(aP):                                 #needed so Paused isn't just written over and over again
            a.text(500000, -1, "Paused", fontsize=10)
            aP = False                          #do not write Paused again until new data is displayed
        
            

class DANDYapp(tk.Tk):
    global chartLoadstr
    global clippingOverrideMenuMessage
    global currentViewMsg
    def __init__(self, *args, **kwargs):
        global chartLoadstr
        global clippingOverrideMenuMessage
        global currentViewMsg
        #initialize device driver
        if(getData):
            subprocess.call(['sudo', 'insmod', './specAn_driver_pi3.ko'])
            subprocess.call(['sudo', 'mknod', '/dev/specAn_driver_pi3', 'c', '243', '0'])
        
        tk.Tk.__init__(self, *args, **kwargs)

        #tk.Tk.iconbitmap(self, default="dandy-logo.ico") #this line often doesn't work so just comment it out
        tk.Tk.wm_title(self, "Dandy Spectrum Analyzer")     #title that shows up in program 
        
        
        container = tk.Frame(self)
        container.pack(side="top", fill="both", expand = True)
        container.grid_rowconfigure(0, weight=1)
        container.grid_columnconfigure(0, weight=1)

        def updateMenu():               #function that updates words in menu if 'postcommand=updateMenu' is used
            global chartLoadstr
            runStop.entryconfig(1, label=chartLoadstr)                      #check to see if Run / Stop should be used
            dispSettings.entryconfig(5, label=clippingWarningMenuMessage)   #Check to see if 'off' of 'on' should be used
            dispSettings.entryconfig(1, label=currentViewMsg)                  #Check to see if 'time' or 'frequency should be displayed
        #Menu Settings
        menubar = tk.Menu(container)
        #File Menu
        filemenu = tk.Menu(menubar, tearoff=0)
        filemenu.add_command(label="Save settings", command = lambda: popupmsg("Not supported yet"))
        filemenu.add_separator()
        filemenu.add_command(label="Exit", command=quit)
        menubar.add_cascade(label="File", menu=filemenu)
        #Data Acquisition Settings Menu
        acqSettings = tk.Menu(menubar, tearoff=1)
        acqSettings.add_command(label="Change Acquisition Method",
                                   command=lambda: changeAcq("m"))
        acqSettings.add_command(label="Change Oscillator Frequency",
                                   command=lambda: changeAcq("o"))
        acqSettings.add_command(label="Change Correction Frequency",
                                   command=lambda: changeAcq("c"))
        menubar.add_cascade(label="Data Acquisition Settings", menu=acqSettings)
        #Gain Settings Menu
        gainSettings = tk.Menu(menubar, tearoff=1)
        gainSettings.add_command(label="Gain = 0",
                                 command=lambda: changeGain('0'))
        gainSettings.add_command(label="Gain = 1",
                                 command=lambda: changeGain('1'))
        gainSettings.add_command(label="Gain = 2",
                                 command=lambda: changeGain('2'))
        gainSettings.add_command(label="Gain = 5",
                                 command=lambda: changeGain('5'))
        gainSettings.add_command(label="Gain = 10",
                                 command=lambda: changeGain('10'))
        gainSettings.add_command(label="Gain = 20",
                                 command=lambda: changeGain('20'))
        gainSettings.add_command(label="Gain = 50",
                                 command=lambda: changeGain('50'))
        gainSettings.add_command(label="Gain = 100",
                                 command=lambda: changeGain('100'))
        menubar.add_cascade(label="Gain Settings", menu=gainSettings)
        #Display Settings Menu
        dispSettings = tk.Menu(menubar, tearoff=1, postcommand=updateMenu)
        dispSettings.add_command(label=currentViewMsg,
                                 command = lambda: disp('t'))
        dispSettings.add_command(label="Change X Axis Scale",
                                 command = lambda: disp('x'))
        dispSettings.add_command(label="Change Y Axis Scale",
                                 command = lambda: disp('y'))
        dispSettings.add_command(label="Change Color",
                                 command = lambda: disp('c'))
        dispSettings.add_command(label=clippingWarningMenuMessage,
                                 command = lambda: disp('co'))
        menubar.add_cascade(label="Display Settings", menu=dispSettings)
        #Measure Settings Menu
        measureSettings = tk.Menu(menubar, tearoff=1)
        measureSettings.add_command(label="Show Peak Frequency",
                            command=lambda: measure('f'))
        measureSettings.add_command(label="Energy in BW",
                            command = lambda: measure('e'))
        measureSettings.add_command(label="Average Energy in BW",
                            command = lambda: measure('a'))
        menubar.add_cascade(label="Measure", menu=measureSettings)

        runStop = tk.Menu(menubar, tearoff=1, postcommand=updateMenu)
        runStop.add_command(label = chartLoadstr,
                            command = lambda: loadChart())
        menubar.add_cascade(label="Run / Stop", menu=runStop)                                      
        #Help Settings Menu
        helpmenu = tk.Menu(menubar, tearoff=0)
        helpmenu.add_command(label="Tutorial",command=tutorial)
        helpmenu.add_command(label="About",command=about)
        menubar.add_cascade(label="Help", menu = helpmenu)
        tk.Tk.config(self, menu=menubar)
        self.frames = {}

        for F in (StartPage, PageOne):
            frame = F(container, self)
            self.frames[F] = frame
            frame.grid(row=0, column=0, sticky="nsew")
        self.show_frame(StartPage)
        
    def show_frame(self, cont):
        frame = self.frames[cont]
        frame.tkraise()

        
class StartPage(tk.Frame):          #Start page of the application
    def __init__(self, parent, controller):
        tk.Frame.__init__(self,parent)
        label = tk.Label(self, text="ALPHA DANDY Spectrum Analyzer", font=LARGE_FONT)
        label.pack(pady=10,padx=10)
        label = tk.Label(self, text="""
        The DANDY Spectrum Analyzer is given with no warranty. Do not make
        any modifications to the electronics inside the enclosure
        without proper supervision and eqiuptment. Use at your own risk.
        By clicking agree, you understand that the following application
        is for educational uses only, you will use proper laboratory precautions
        when dealing with electronic signals placed in this spectrum analyzer
        and you will not find the authors of this project liable for any damages.""", font=LARGE_FONT)
        label.pack(pady=10, padx=10)
        button1 = ttk.Button(self, text="Agree",
                            command=lambda: controller.show_frame(PageOne))
        button1.pack()
        button2 = ttk.Button(self, text="Disagree",
                            command=quit)
        button2.pack()
class PageOne(tk.Frame):
    global RunStatus
    def __init__(self, parent, controller):
        tk.Frame.__init__(self, parent)  
       
        canvas = FigureCanvasTkAgg(f, self)
        canvas.show()
        canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True)

        toolbar = NavigationToolbar2TkAgg(canvas, self)
        toolbar.update()
        canvas._tkcanvas.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
app = DANDYapp()
app.geometry("800x480") #screen resolution of 7" touchscreen
ani = animation.FuncAnimation(f, animate, interval=250) #last argument sets time between animations (in milliseconds)
app.mainloop()
