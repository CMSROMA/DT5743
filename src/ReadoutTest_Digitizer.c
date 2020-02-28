#include <stdio.h>
#include "CAENDigitizer.h"

#include "keyb.h"

#define CAEN_USE_DIGITIZERS
#define IGNORE_DPP_DEPRECATED

#define MAXNB 1 /* Number of connected boards */

int checkCommand() {
  int c = 0;
  if(!kbhit())
    return 0;

  c = getch();
  switch (c) {
  case 's': 
    return 9;
    break;
  case 'k':
    return 1;
    break;
  case 'q':
    return 2;
    break;
  }
  return 0;
}


static long get_time()
{
    long time_ms;
    struct timeval t1;
    struct timezone tz;
    gettimeofday(&t1, &tz);
    time_ms = (t1.tv_sec) * 1000 + t1.tv_usec / 1000;
    return time_ms;
}

int writeOutputFilesx743(FILE* file, CAEN_DGTZ_EventInfo_t *EventInfo, CAEN_DGTZ_X743_EVENT_t *Event)
{
  int ch, j, ns;
  // Binary file format
  uint32_t BinHeader[5];
  BinHeader[0] = 1024*4*2 + 5*sizeof(*BinHeader) + 2*sizeof(*BinHeader) ; //Nsamples* float size * 2 channels + 5*uint32 +  2*chHeader(uint32)
  BinHeader[1] = EventInfo->BoardId;
  BinHeader[2] = EventInfo->Pattern;
  BinHeader[4] = EventInfo->EventCounter;
  BinHeader[5] = EventInfo->TriggerTimeTag;

  int Size = Event->DataGroup[0].ChSize;
  if (Size != 1024) 
    return -1;

  fwrite(BinHeader, sizeof(*BinHeader), 5, file);  
  for(ch=0; ch<2; ch++) {
    uint32_t ChHeader;
    fwrite(&ChHeader, sizeof(ChHeader), 1, file);
    
    ns = (int)fwrite( Event->DataGroup[0].DataChannel[ch] , 1 , Size*4, file) / 4;
    if (ns != 1024) {
      // error writing to file
      fclose(file);
      return -1;
    }
  }

  return 0;
}

int main(int argc, char* argv[])
{
  /* The following variable is the type returned from most of CAENDigitizer
     library functions and is used to check if there was an error in function
     execution. For example:
     ret = CAEN_DGTZ_some_function(some_args);
     if(ret) printf("Some error"); */
  CAEN_DGTZ_ErrorCode ret;

  /* The following variable will be used to get an handler for the digitizer. The
     handler will be used for most of CAENDigitizer functions to identify the board */
  int	handle[MAXNB];

  CAEN_DGTZ_BoardInfo_t BoardInfo;
  CAEN_DGTZ_EventInfo_t eventInfo;
  CAEN_DGTZ_UINT16_EVENT_t *Evt = NULL;
  char *buffer = NULL;
  int MajorNumber;
  int i,b;
  int c = 0, count[MAXNB], Nb[MAXNB], Ne[MAXNB];
  char * evtptr = NULL;
  uint32_t size,bsize;
  uint32_t numEvents=0;
  i = sizeof(CAEN_DGTZ_TriggerMode_t);


  for(b=0; b<MAXNB; b++){
    /* IMPORTANT: The following function identifies the different boards with a system which may change
       for different connection methods (USB, Conet, ecc). Refer to CAENDigitizer user manual for more info.
       brief:
       CAEN_DGTZ_OpenDigitizer(<LikType>, <LinkNum>, <ConetNode>, <VMEBaseAddress>, <*handler>);
       Some examples below */
        
    /* The following is for b boards connected via b USB direct links
       in this case you must set <LikType> = CAEN_DGTZ_USB and <ConetNode> = <VMEBaseAddress> = 0 */
    //ret = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB, b, 0, 0, &handle[b]);

    /* The following is for b boards connected via 1 opticalLink in dasy chain
       in this case you must set <LikType> = CAEN_DGTZ_PCI_OpticalLink and <LinkNum> = <VMEBaseAddress> = 0 */
    //ret = CAEN_DGTZ_OpenDigitizer(Params[b].LinkType, 0, b, Params[b].VMEBaseAddress, &handle[b]);

    /* The following is for b boards connected to A2818 (or A3818) via opticalLink (or USB with A1718)
       in this case the boards are accessed throught VME bus, and you must specify the VME address of each board:
       <LikType> = CAEN_DGTZ_PCI_OpticalLink (CAEN_DGTZ_PCIE_OpticalLink for A3818 or CAEN_DGTZ_USB for A1718)
       <LinkNum> must be the bridge identifier
       <ConetNode> must be the port identifier in case of A2818 or A3818 (or 0 in case of A1718)
       <VMEBaseAddress>[0] = <0xXXXXXXXX> (address of first board) 
       <VMEBaseAddress>[1] = <0xYYYYYYYY> (address of second board) 
       ...
       <VMEBaseAddress>[b-1] = <0xZZZZZZZZ> (address of last board)
       See the manual for details */
    ret = CAEN_DGTZ_OpenDigitizer(CAEN_DGTZ_USB,0,0,0,&handle[b]);
    if(ret != CAEN_DGTZ_Success) {
      printf("Can't open digitizer\n");
      goto QuitProgram;
    }
    /* Once we have the handler to the digitizer, we use it to call the other functions */
    ret = CAEN_DGTZ_GetInfo(handle[b], &BoardInfo);
    printf("\nConnected to CAEN Digitizer Model %s, recognized as board %d\n", BoardInfo.ModelName, b);
    printf("\tROC FPGA Release is %s\n", BoardInfo.ROC_FirmwareRel);
    printf("\tAMC FPGA Release is %s\n", BoardInfo.AMC_FirmwareRel);
	    
    // Check firmware revision (DPP firmwares cannot be used with this demo */
    sscanf(BoardInfo.AMC_FirmwareRel, "%d", &MajorNumber);
    if (MajorNumber >= 128) {
      printf("This digitizer has a DPP firmware!\n");
      goto QuitProgram;
    }

    ret = CAEN_DGTZ_Reset(handle[b]);                                               /* Reset Digitizer */
    ret |= CAEN_DGTZ_GetInfo(handle[b], &BoardInfo);                                 /* Get Board Info */
    ret |= CAEN_DGTZ_SetSAMPostTriggerSize(handle[b], 0, 50);
    ret |= CAEN_DGTZ_SetSAMPostTriggerSize(handle[b], 1, 50);
    ret |= CAEN_DGTZ_SetSAMPostTriggerSize(handle[b], 2, 50);
    ret |= CAEN_DGTZ_SetSAMPostTriggerSize(handle[b], 3, 50);
    if(ret != 0) printf("Error setting post trigger: %i \n",ret);
    ret |= CAEN_DGTZ_SetSAMSamplingFrequency(handle[b], 0);
    if (ret != 0) printf("Error setting frequency: %i\n",ret);
    // Other SAMLONG digitizatoin parameters
    ret |= CAEN_DGTZ_SetSAMCorrectionLevel(handle[b], CAEN_DGTZ_SAM_CORRECTION_ALL);
    //ret |= CAEN_DGTZ_SetSAMCorrectionLevel(handle[b], CAEN_DGTZ_SAM_CORRECTION_DISABLED);
    if(ret != 0) printf("Error setting CorLevel: %i\n",ret);
    ret |= CAEN_DGTZ_LoadSAMCorrectionData(handle[b]);
    if(ret != 0) printf("Error setting Correction Data: %i\n",ret);
    ret |= CAEN_DGTZ_DisableSAMPulseGen(handle[b],0);
    if(ret != 0) printf("Error setting PulseGen: %i\n",ret);
    ret |= CAEN_DGTZ_SetSAMAcquisitionMode(handle[b], CAEN_DGTZ_AcquisitionMode_STANDARD);
    if(ret != 0) printf("Error setting Acq Mode: %i\n",ret);
    // Set the DC offset
    int ich=0;
    for(ich = 0; ich < 8; ich++){
      ret |= CAEN_DGTZ_SetChannelDCOffset(handle[b],ich, 32768); 
      if(ret != 0 && ret != -17 ) printf("Error setting DAC: %i\n",ret);
    }
    ret |= CAEN_DGTZ_SetRecordLength(handle[b],1024);                                /* Set the lenght of each waveform (in samples) */

    // enable the external trigger
    //    ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle[b], CAEN_DGTZ_TRGMODE_ACQ_AND_EXTOUT);
    //ret |= CAEN_DGTZ_SetExtTriggerInputMode(handle[b], CAEN_DGTZ_TRGMODE_DISABLED);
  
    //Enable the trigger on channel
    ret |= CAEN_DGTZ_SetChannelEnableMask(handle[b],1);   /* Enable channel 0 */
    ret |= CAEN_DGTZ_SetChannelSelfTrigger(handle[b],CAEN_DGTZ_TRGMODE_ACQ_ONLY,1);

  
    ret |= CAEN_DGTZ_SetChannelTriggerThreshold(handle[b],0,35000); //threshold = value - baseline
    ret |= CAEN_DGTZ_SetTriggerPolarity(handle[b],0,CAEN_DGTZ_TriggerOnFallingEdge);
  
    /* ret |= CAEN_DGTZ_SetChannelTriggerThreshold(handle[b],1,25000); */
    /* ret |= CAEN_DGTZ_SetTriggerPolarity(handle[b],1,CAEN_DGTZ_TriggerOnRisingEdge); */
  
  
    // Use NIM IO for trigger in.
    ret |= CAEN_DGTZ_WriteRegister(handle[b],0x811c,0x0);
  
    // Use TTL IO for trigger in.
    //    ret |= CAEN_DGTZ_WriteRegister(handle[b],0x811c,0x1);

    ret |= CAEN_DGTZ_SetSWTriggerMode(handle[b],CAEN_DGTZ_TRGMODE_ACQ_ONLY);         /* Set the behaviour when a SW tirgger arrives */
    ret |= CAEN_DGTZ_SetMaxNumEventsBLT(handle[b],1);                                /* Set the max number of events to transfer in a sigle readout */
    ret |= CAEN_DGTZ_SetAcquisitionMode(handle[b],CAEN_DGTZ_SW_CONTROLLED);          /* Set the acquisition mode */
    printf ("config status %d\n",ret);

    sleep(1);

    printf("Check config values on DT5743: \n");
    uint32_t dcoffset;
    ret |= CAEN_DGTZ_GetRecordLength(handle[b],&dcoffset);
    printf("record length: %d\n",dcoffset);
    //  ret |= CAEN_DGTZ_GetSAMSamplingFrequency(handle[b],&dcoffset);
    //  printf("sampling frequency: %d\n",dcoffset);
    ret |= CAEN_DGTZ_GetChannelDCOffset(handle[b],0,&dcoffset);
    printf("Channel0 DC offset: %d\n",dcoffset);
    ret |= CAEN_DGTZ_GetChannelDCOffset(handle[b],1,&dcoffset);
    printf("Channel1 DC offset: %d\n",dcoffset);
    ret |= CAEN_DGTZ_GetChannelDCOffset(handle[b],2,&dcoffset);
    printf("Channel2 DC offset: %d\n",dcoffset);
    ret |= CAEN_DGTZ_GetChannelDCOffset(handle[b],3,&dcoffset);
    printf("Channel3 DC offset: %d\n",dcoffset);
  
    printf ("config status is %d\n",ret);

    //    ret |= CAEN_DGTZ_SetAnalogMonOutput(handle[b],CAEN_DGTZ_AM_TRIGGER_MAJORITY);
  
    if(ret != CAEN_DGTZ_Success) {
      printf("Errors during Digitizer Configuration.\n");
      goto QuitProgram;
    }
  }
  printf("\n\nPress 's' to start the acquisition\n");
  printf("Press 'k' to stop  the acquisition\n");
  printf("Press 'q' to quit  the application\n\n");
  while (1) {
    c = checkCommand();
    if (c == 9) break;
    if (c == 2) return;
    Sleep(100);
  }
  /* Malloc Readout Buffer.
     NOTE1: The mallocs must be done AFTER digitizer's configuration!
     NOTE2: In this example we use the same buffer, for every board. We
     Use the first board to allocate the buffer, so if the configuration
     is different for different boards (or you use different board models), may be
     that the size to allocate must be different for each one. */

  uint32_t AllocatedSize, BufferSize, NumEvents;
  CAEN_DGTZ_X743_EVENT_t    *Event16 = NULL;
  ret = CAEN_DGTZ_AllocateEvent(handle[0], (void**)&Event16);
  ret |= CAEN_DGTZ_MallocReadoutBuffer(handle[0],&buffer,&AllocatedSize);
  if (ret != CAEN_DGTZ_Success) {
    printf("Error Malloc\n");
  }
    
  for(b=0; b<MAXNB; b++)
    /* Start Acquisition
       NB: the acquisition for each board starts when the following line is executed
       so in general the acquisition does NOT starts syncronously for different boards */
    ret = CAEN_DGTZ_SWStartAcquisition(handle[b]);

  FILE* fout=fopen("wave.dat","w");

  if (fout != NULL)
    printf("Opened output file\n");
  else
    {
      printf("Error opening file. Exit\n");
      goto QuitProgram;
    }

  // Start acquisition loop
  uint32_t data;
  uint64_t CurrentTime, PrevRateTime, ElapsedTime;
  PrevRateTime = get_time();
  while(1) {
    for(b=0; b<MAXNB; b++) {

      /* ret = CAEN_DGTZ_SendSWtrigger(handle[b]); /\* Send a SW Trigger *\/ */

      /* Wait for an event to be digitised */
      int lam=0;
      while(!lam)
	{
	    // Read the correct register to check number of events stored on digitizer.
	    CAEN_DGTZ_ReadRegister(handle[b],0x812c,&data);
	    if(data > 0) lam = 1;
	    usleep(10);
	}

      /* The buffer read from the digitizer is used in the other functions to get the event data
	 The following function returns the number of events in the buffer */

      ret = CAEN_DGTZ_ReadData(handle[b],CAEN_DGTZ_SLAVE_TERMINATED_READOUT_MBLT,buffer,&BufferSize); /* Read the buffer from the digitizer */
      ret = CAEN_DGTZ_GetNumEvents(handle[b],buffer,BufferSize,&NumEvents);
      count[b] +=NumEvents;
      Nb[b] += BufferSize;
      Ne[b] += NumEvents;

      for (i=0;i<NumEvents;i++) {
	/* Get the Infos and pointer to the event */
	ret = CAEN_DGTZ_GetEventInfo(handle[b],buffer,BufferSize,i,&eventInfo,&evtptr);
	if (ret)
	  printf ("Error GetEventInfo\n");

	/* Decode the event to get the data */
	ret = CAEN_DGTZ_DecodeEvent(handle[b],evtptr,(void**)&Event16);
	if (ret)
	  printf ("Error DecodeEvent\n");

	//*************************************
	// Write Event in binary format
	//*************************************
	writeOutputFilesx743(fout,&eventInfo,Event16);

	if (count[b]%100!=0)
	  continue;
	  
	printf("+++++++++++++++++++++++++++++++++\n");
	printf("Event Number: %d\n", eventInfo.EventCounter+1);
	printf("Trigger Time Stamp: %u\n", eventInfo.TriggerTimeTag);
        CurrentTime = get_time();
        ElapsedTime = CurrentTime - PrevRateTime;
	printf("Reading at %.2f MB/s (Trg Rate: %.2f Hz)\n", (float)Nb[b]/((float)ElapsedTime*1048.576f), (float)Ne[b]*1000.0f/(float)ElapsedTime);
	Nb[b] = 0;
	Ne[b] = 0;
	PrevRateTime = CurrentTime;
      }
      c = checkCommand();
      if (c == 1) goto Continue;
      if (c == 2) goto Continue;
    } // end of loop on boards
  } // end of readout loop

 Continue:
  for(b=0; b<MAXNB; b++)
    printf("\nBoard %d: Retrieved %d Events\n",b, count[b]);
  goto QuitProgram;

  /* Quit program routine */
 QuitProgram:
  // Free the buffers and close the digitizers
  ret = CAEN_DGTZ_FreeReadoutBuffer(&buffer);
  for(b=0; b<MAXNB; b++)
    ret = CAEN_DGTZ_CloseDigitizer(handle[b]);
  printf("Press 'Enter' key to exit\n");
  c = getchar();
  return 0;
}

