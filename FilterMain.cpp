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

  // Initialized col, row, i, j, mrow, mcol, color0, color1, color2 before the loop
  // This will speed things up because the system can use 
  int col, row, i, j;

  // Initialized height as height = (input -> height) - 1;
  // This will speed things by allowing to system to access input once an make the
  // value available to the loops.
  int height = (input -> height) - 1;

  // Initialized height as width = (input -> width) - 1;
  // Same as with height.
  int width = (input -> width) - 1;

  // Initialized divisor as divisor = filter -> getDivisor();
  // This will speed things up by calling the getDivisor function only once
  // and saving the return for use in the loop. This is a big savings as there
  // will only need to be the one function call instead of a great many.
  char divisor = filter -> getDivisor();

  // Initialize *data as filter -> data so that we have easier access to the now
  // public variable *data. This will be used in the loop so we want quick access.
  // *data was switched to public so we don't need a function call to access it
  int *data = filter -> data;
  
  // #pragma omp parallel for is used with openMP this will optimize the loop
  // in the compiler.
  #pragma omp parallel for

  /*
  Removed the plane loop as it's not needed. It's values are 0, 1, 2 so instead
  of color[plane] we can do the following:
  color[0]
  color[1] 
  color[2]
  
  Also made the outer loop row instead of col. This is b/c 3d array for color
  is color[?][row][col]. This way inner loop of the two is col thus we won't have
  as many cache misses.
  */
  for(row = 1; row < height; row++) {
    for(col = 1; col < width; col++) {

      
      // Instead of having output -> color[?][row][col] be set to zero each loop
      // the code now initializes 3 color variables and sets them to zero.
      int color0, color1, color2;
      color0 = color1 = color2 = 0;

      // made the outer loop i instead of j. This is b/c 3d array for color is
      // color[?][row - 1 + i][col - 1 + j] . This way inner loop of the two is j  
      // thus we won't have as many cache misses. filter -> getSize() is changed to
      // 3 as all of the filters are size 3.
      for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {	

          // Set each color to itself + (input -> color[?][row - 1 + i][col - 1 + j] 
          // * data[i * 3 + j]). The [?] comes from fully unrolling the plane loop
          // and the data[i * 3 + j] is just doing what calling filter -> get(i, j)
          // does without making a function call. 
          color0 += (input -> color[0][row - 1 + i][col - 1 + j] * data[i * 3 + j]);
          color1 += (input -> color[1][row - 1 + i][col - 1 + j] * data[i * 3 + j]);
          color2 += (input -> color[2][row - 1 + i][col - 1 + j] * data[i * 3 + j]);
        }
      }
	
      // Removed the output -> color[plane][row][col] = output -> 
      // color[plane][row][col] / filter -> getDivisor() for two reasons. First
      // we are waiting to save to the output at the end, thus the color? variables.
      // Two earlier outside of the loop we made a variable to get the divisor.
      // This way we greatly reduce the number of function calls.
      color0 /= divisor;
      color1 /= divisor;  
      color2 /= divisor;

      // Switched all of the if's to ternary operators. When they were if's some
      // of the fiters still ran incredibly fast when openMP was added but other
      // filters were greatly slowed down. I suspect the reason for that is the 
      // ternaries help with branching when openMP is on.
      color0 = (color0 < 0) ? 0 : color0;
      color1 = (color1 < 0) ? 0 : color1;
      color2 = (color2 < 0) ? 0 : color2;

      color0 = (color0 > 255) ? 255 : color0;
      color1 = (color1 > 255) ? 255 : color1;
      color2 = (color2 > 255) ? 255 : color2;

      // Since we accumulated all of the changes to the pixel color values in int
      // variable when now need to set the output values the color variables.
      output -> color[0][row][col] = color0;
      output -> color[1][row][col] = color1;
      output -> color[2][row][col] = color2;
    }
  }
 

  cycStop = rdtscll();
  double diff = cycStop - cycStart;
  double diffPerPixel = diff / (output -> width * output -> height);
  fprintf(stderr, "Took %f cycles to process, or %f cycles per pixel\n",
	  diff, diff / (output -> width * output -> height));
  return diffPerPixel;
}
