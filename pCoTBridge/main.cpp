/************************************************************/
/*    NAME: Tyler Errico                                    */
/*    ORGN: West Point Robotics Research Center, USMA       */
/*    FILE: main.cpp                                        */
/*    DATE: April 2026                                      */
/************************************************************/

#include <iostream>
#include "MBUtils.h"
#include "ColorParse.h"
#include "CoTBridge.h"
#include "CoTBridge_Info.h"

using namespace std;

int main(int argc, char* argv[])
{
  string mission_file;
  string run_command = argv[0];

  for(int i = 1; i < argc; i++) {
    string argi = argv[i];
    if((argi == "-v") || (argi == "--version"))
      showReleaseInfoAndExit();
    else if((argi == "-e") || (argi == "--example"))
      showExampleConfigAndExit();
    else if((argi == "-h") || (argi == "--help"))
      showHelpAndExit();
    else if((argi == "-i") || (argi == "--interface"))
      showInterfaceAndExit();
    else if(strEnds(argi, ".moos") || strEnds(argi, ".moos++"))
      mission_file = argi;
    else if(strBegins(argi, "--alias="))
      run_command = argi.substr(8);
    else if(i == 2)
      run_command = argi;
  }

  if(mission_file.empty())
    showHelpAndExit();

  cout << termColor("green");
  cout << "pCoTBridge launching as " << run_command << endl;
  cout << termColor() << endl;

  CoTBridge bridge;
  bridge.Run(run_command.c_str(), mission_file.c_str(), argc, argv);

  return 0;
}
