#include <stdio.h>
#include "cs1300bmp.h"
#include <iostream>
#include <fstream>
#include <stdlib.h>
#include "Filter.h"



using namespace std;

#include "rdtsc.h"

//
// Forward declare the functions
//
Filter * readFilter(string filename);
double applyFilter(Filter *filter, cs1300bmp *input, cs1300bmp *output);

int
main(int argc, char **argv)
{

  if ( argc < 2) {
    fprintf(stderr,"Usage: %s filter inputfile1 inputfile2 .... \n", argv[0]);
  }

  //
  // Convert to C++ strings to simplify manipulation
  //
  string filtername = argv[1];

  //
  // remove any ".filter" in the filtername
  //
  string filterOutputName = filtername;
  string::size_type loc = filterOutputName.find(".filter");
  if (loc != string::npos) {
    //
    // Remove the ".filter" name, which should occur on all the provided filters
    //
    filterOutputName = filtername.substr(0, loc);
  }

  Filter *filter = readFilter(filtername);

  double sum = 0.0;
  int samples = 0;

  for (int inNum = 2; inNum < argc; inNum++) {
    string inputFilename = argv[inNum];
    string outputFilename = "filtered-" + filterOutputName + "-" + inputFilename;
    struct cs1300bmp *input = new struct cs1300bmp;
    struct cs1300bmp *output = new struct cs1300bmp;
    int ok = cs1300bmp_readfile( (char *) inputFilename.c_str(), input);

    if ( ok ) {
      double sample = applyFilter(filter, input, output);
      sum += sample;
      samples++;
      cs1300bmp_writefile((char *) outputFilename.c_str(), output);
    }
    delete input;
    delete output;
  }
  fprintf(stdout, "Average cycles per sample is %f\n", sum / samples);

}

class Filter *
readFilter(string filename)
{
  ifstream input(filename.c_str());

  if ( ! input.bad() ) {
    int size = 0;
    input >> size;
    Filter *filter = new Filter(size);
    int div;
    input >> div;
    filter -> setDivisor(div);
    for (int i=0; i < size; i++) {
      for (int j=0; j < size; j++) {
	int value;
	input >> value;
	filter -> set(i,j,value);
      }
    }
    return filter;
  } else {
    cerr << "Bad input in readFilter:" << filename << endl;
    exit(-1);
  }
}


double
applyFilter(class Filter *filter, cs1300bmp *input, cs1300bmp *output)
{

  long long cycStart, cycStop;

  cycStart = rdtscll();

  output -> width = input -> width;
  output -> height = input -> height;

  // Declared color0, color1, color2 as shorts
  short color0, color1, color2;
  int col, row, i, j, mrow, mcol;
  int height = (input -> height) - 1;
  int width = (input -> width) - 1;
  char divisor = filter -> getDivisor();
  int filterSize = filter -> getSize();
  int *data = filter -> data;
  
  //for(int plane = 0; plane < 3; plane++) {
  if (divisor == 1) {
  
  for(row = 1; row < height; row++) {
    for(col = 1; col < width; col++) {

      //output -> color[0][row][col] = 0;
      //output -> color[1][row][col] = 0;
      //output -> color[2][row][col] = 0;
      // Instead of having output -> color[?][row][col] be set to zero each loop
      // the code now sets 3 short values to 0.
      mrow = row - 1;
      mcol = col - 1;
      color0 = color1 = color2 = 0;

      
      for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {	
          color0 += (input -> color[0][mrow + i][mcol + j] 
            *  data[i * 3 + j] );
          color1 += (input -> color[1][mrow + i][mcol + j] 
            *  data[i * 3 + j] );
          color2 += (input -> color[2][mrow + i][mcol + j] 
            *  data[i * 3 + j] );
        }
      }
	
      

      if (color0 < 0) {
        color0 = 0;
      }

      if (color1 < 0) {
        color1 = 0;
      }

      if (color2 < 0 ) {
        color2 = 0;
      }

      if (color0  > 255) { 
        color0 = 255;
      }
      if (color1 > 255) { 
        color1 = 255;
      }
      if (color2 > 255 ) { 
        color2 = 255;
      }

      output -> color[0][row][col] = color0;
      output -> color[1][row][col] = color1;
      output -> color[2][row][col] = color2;
    }
  }
  }
  else {
  
  for(row = 1; row < height; row++) {
    for(col = 1; col < width; col++) {

      mrow = row - 1;
      mcol = col - 1;
      //output -> color[0][row][col] = 0;
      //output -> color[1][row][col] = 0;
      //output -> color[2][row][col] = 0;
      color0 = color1 = color2 = 0;

      
      for (i = 0; i < filterSize; i++) {
        for (j = 0; j < filterSize; j++) {	
          color0 += (input -> color[0][mrow + i][mcol + j] 
            * data[i * 3 + j] );
          color1 += (input -> color[1][mrow + i][mcol + j] 
            * data[i * 3 + j] );
          color2 += (input -> color[2][mrow + i][mcol + j] 
            * data[i * 3 + j] );
        }
      }
	
      color0 = color0 / divisor;
      color1 = color1 / divisor;  
      color2 = color2 / divisor;

      if (color0 < 0) {
        color0 = 0;
      }

      if (color1 < 0) {
        color1 = 0;
      }

      if (color2 < 0 ) {
        color2 = 0;
      }

      if (color0  > 255) { 
        color0 = 255;
      }
      if (color1 > 255) { 
        color1 = 255;
      }
      if (color2 > 255 ) { 
        color2 = 255;
      }

      output -> color[0][row][col] = color0;
      output -> color[1][row][col] = color1;
      output -> color[2][row][col] = color2;
    }
  }
  }
  //}

  cycStop = rdtscll();
  double diff = cycStop - cycStart;
  double diffPerPixel = diff / (output -> width * output -> height);
  fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (output -> width * output -> height));
  return diffPerPixel;
}
