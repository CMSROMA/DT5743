/*
   Name: d75743convert.cxx

   Purpose:  Convert binary into H4DAQ raw ROOT format 
*/

#include <cstring>
#include <math.h>
#include <fstream>
#include <vector>
#include <time.h>

#include "TFile.h"
#include "TTree.h"
#include "TSystem.h"
#include "TString.h"
#include "TRegexp.h"

#include "Event.h"

#define MAX_PROCESSED_SPILL_PER_FILE 5

typedef struct {
  unsigned int eventSize;
  unsigned int boardId;
  unsigned int pattern;
  unsigned int eventCounter;
  unsigned int triggerTimeTag;
} EHEADER;

typedef struct {
  unsigned int chId;
} CHEADER;


/*-----------------------------------------------------------------------------*/

using namespace H4DAQ;

TFile * outFile = NULL;
TTree * outTree = NULL;
H4DAQ::Event* event_ = NULL;

void createOutputFile(TString outName, int seq)
{
  if (outFile)
    {
      outFile->ls () ;
      outFile->cd () ;
      if (outTree)
	outTree->Write ("",TObject::kOverwrite) ;
      outFile->Close () ;
    }
  outFile = TFile::Open (Form("%s/h4Tree_%d.root",outName.Data(),seq), "RECREATE") ;  

  if (!outFile->IsOpen ()) 
    std::cout << "Cannot open " << outFile->GetName() << std::endl;
   
  outTree = new TTree ("H4tree", "H4 testbeam tree");

  if (event_)
    delete event_;
  event_ = new H4DAQ::Event(outTree) ;
}


int main(int argc, char **argv)
{
   EHEADER  eh;
   CHEADER  ch;
   
   float waveform[2][1024]; //saving only 2 channels for the DT5743
   int b, n_boards;
   int chn_index[2];
   TString inpath,outName,spills;
   
   int prescale=-1;
   bool allFiles=false;

   int c;
   opterr = 0;

   while ((c = getopt (argc, argv, "i:o:s:p:a")) != -1)
     switch (c)
       {
       case 'i':
	 inpath = TString(optarg);
	 break;
       case 'o':
	 outName = TString(optarg);
	 break;
       case 's':
	 spills = TString(optarg);
	 break;
       case 'p':
	 prescale = TString(optarg).Atoi();
	 break;
       case 'a':
	 allFiles=true;
	 break;
       // case '?':
       // 	 if (optopt == 'c')
       // 	   fprintf (stderr, "Option -%c requires an argument.\n", optopt);
       // 	 else if (isprint (optopt))
       // 	   fprintf (stderr, "Unknown option `-%c'.\n", optopt);
       // 	 else
       // 	   fprintf (stderr,
       // 		    "Unknown option character `\\x%x'.\n",
       // 		    optopt);
       // 	 return 1;
       // default:
       // 	 abort ();
       }

   
   string ls_command;
   string path;
   
   std::vector<TString>filenames;
   //---Get file list searching in specified path (eos or locally)
   ls_command = string("ls "+inpath+" | grep '.dat' > /tmp/drs4convert.list");
   system(ls_command.c_str());
   
   ifstream waveList(string("/tmp/drs4convert.list").c_str(), ios::in);
   TString line;
   while (!waveList.eof()) {
     line.ReadLine(waveList);
     if (line.Sizeof() <= 1) continue;
     filenames.push_back(inpath+"/"+TString(line.Strip(TString::kTrailing, ' ')));
     std::cout << "+++ Adding file " << filenames.back() << std::endl;
   }
   
   // ifstream infile(infilename.Data());

   // if (infile.fail()) {
   //   std:cout << "Cannot open list file " << infilename.Data() << std::endl;
   //   return -1;
   // }

   // TString line;
   // const TRegexp number_patt("[-+ ][0-9]*[.]?[0-9]+");
   // while (!infile.eof()) {
   //   line.ReadLine(infile);
   //   if (line.Sizeof() <= 1) continue;
   //   filenames.push_back(new TString(line.Strip(TString::kTrailing, ' ')));
   // }


   // infile.close();
   // infile.clear();

   // std::cout << "Processing files:\n";
   // for (int ifile=0; ifile<filenames.size(); ifile++) {
   //   std::cout << filenames.at(ifile).Data() << std::endl;
   // }
   // std::cout << "Spike removal " << (removeSpikes ? "active.\n" : "inactive.\n");
    
   if (!gSystem->OpenDirectory(outName.Data()))
      gSystem->mkdir(outName.Data());
   
   
   std::set<int> spillNumbers;
   Ssiz_t from = 0;
   TString tok;
   while (spills.Tokenize(tok, from, ",")) {
     spillNumbers.insert(tok.Atoi());
   }

   int startTime=-999;
   int processedSpills=0;
   /*** Prepare tree and output histos ***/
   for (unsigned ifile=0; ifile<filenames.size(); ifile++) {

     if (!allFiles && spillNumbers.size())
       {
	 TString baseName(gSystem->BaseName(filenames.at(ifile).Data()));
	 TString spill;
	 Ssiz_t from = 0;
	 baseName.Tokenize(spill, from, "_");
	 if (spillNumbers.find(spill.Atoi()) == spillNumbers.end() )
	   continue;
       }

     if (processedSpills%MAX_PROCESSED_SPILL_PER_FILE==0)
       createOutputFile(outName,processedSpills/MAX_PROCESSED_SPILL_PER_FILE +1);

     std::cout << "Opening data file \'" << filenames.at(ifile).Data() << "\'\n";

     ifstream infile;
     infile.open(filenames.at(ifile).Data(), std::ifstream::binary);
     if (infile.fail()) {
       std::cout << "Cannot open!\n";
       break;
      }

     processedSpills++;     
     n_boards = 1;

     eh.eventCounter = 0;
     while (eh.eventCounter < 1000000) {
 
       // read event header
        infile.read(reinterpret_cast<char*>(&eh), sizeof(eh));
        if (infile.fail()) break;

	event_->clear();
	event_->id.runNumber = 1;
	event_->id.spillNumber = 1;
	event_->id.evtNumber = eh.eventCounter;

	//fill event time
	timeData td; td.board=1;
	td.time=eh.triggerTimeTag;

	if (startTime==-999)
	  {
	    startTime=td.time;
	    td.time=0;
	  }
	else
	  {
	    td.time= td.time - startTime;
	  }

	event_->evtTimes.push_back(td);
				   
	// struct tm * timeinfo;
	// time_t ts = time(NULL);
	// timeinfo = localtime(&ts);
	// timeinfo->tm_year   = eh.year - 1900;
	// timeinfo->tm_mon    = eh.month - 1;    //months since January - [0,11]
	// timeinfo->tm_mday   = eh.day;          //day of the month - [1,31] 
	// timeinfo->tm_hour   = eh.hour;         //hours since midnight - [0,23]
	// timeinfo->tm_min    = eh.minute;          //minutes after the hour - [0,59]
	// timeinfo->tm_sec    = eh.second;          //seconds after the minute - [0,59]
	
	// ts = mktime ( timeinfo );

	// timeData td1; td.board=1;
	// td1.time=ts;
	// event_->evtTimes.push_back(td1);
	
        if (eh.eventCounter%100 == 0) {
          std::cout << Form("Found event #%d triggerTime:%d\n", eh.eventCounter, td.time);
        }

        // loop over all boards in data file
        for (b=0 ; b<n_boards ; b++) {

           // read channel data
           for (unsigned ichan=0 ; ichan<2 ; ichan++) {

              // read channel header and voltagee
             infile.read(reinterpret_cast<char*>(&ch), sizeof(ch));
	     chn_index[ichan] = ch.chId;
	     infile.read(reinterpret_cast<char*>(waveform[ichan]), sizeof(float)*1024);
           }

           // Process data in all channels
           for (unsigned ichan=0 ; ichan<2 ; ichan++) {
             int chidx = chn_index[ichan];
	     for (int ibin=0 ; ibin<1024 ; ibin++) {
	       digiData aDigiSample ;
	       aDigiSample.board = b;
	       aDigiSample.channel = chidx;
	       aDigiSample.group = 0;
	       aDigiSample.frequency = 0; //3.2Gs/s
	       aDigiSample.startIndexCell = 0; //not stored, assign dummy
	       aDigiSample.sampleIndex = ibin;
	       aDigiSample.sampleTime = ibin*0.3125; //3.2Gs/s [ns]
	       aDigiSample.sampleRaw = 0xFFFF; //put dummy value
	       aDigiSample.sampleValue = waveform[ichan][ibin];
	       event_->digiValues.push_back (aDigiSample) ;
	     }
	     
           } // Loop over channels
        } // Loop over the boards

	if ( (prescale < 0) || ( (prescale>0) && eh.eventCounter%prescale==0))
	  event_->Fill();
      } // Loop over events

      infile.close();
      infile.clear();
      //      std::cout << "Read " << eh.eventCounter << " events from this file. Event tree has a total of " << events.GetEntries() << " entries.\n";
   } // Loop over files
   
   
   outFile->ls () ;
   outFile->cd () ;
   outTree->Write ("",TObject::kOverwrite) ;
   outFile->Close () ;

   delete event_;
   return 1;
}



