/*
*	File : dosift.cpp
*	Author : Émile Robitaille @ LERobot
*	Creation date : 2014, June 27th
*	Version : 1.0
*	
*	Description : Functions relative to sift
*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <string>
#include <vector>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/features2d/features2d.hpp>
#if CV_VERSION_MAJOR == 2
#include <opencv2/nonfree/nonfree.hpp>
#elif CV_VERSION_MAJOR == 3
#include <opencv2/xfeatures2d.hpp>
#include <opencv2/xfeatures2d/nonfree.hpp>
#endif

#include "directory.h"
#include "dosift.h"
#include "util.h"

using namespace std;
using namespace cv;
#if CV_VERSION_MAJOR == 3
using namespace xfeatures2d;
#endif


/* CV_LOAD_IMAGE_GRAYSCALE is renamed to IMREAD_GRAYSCALE in OpenCV 3 */
#if CV_VERSION_MAJOR == 3
    #define CV_LOAD_IMAGE_GRAYSCALE IMREAD_GRAYSCALE
#endif


/* 
*	Function : doSift
*	Description : Find sift points on the image
*	
*	path : path of the image
*	container : container for sift keypoints and their descriptor
*/
void doSift(const string &path, struct SFeatures &container)
{
	Mat img, des;
	vector<KeyPoint> keypoints;

	img = imread(path.c_str(), CV_LOAD_IMAGE_GRAYSCALE);

	SiftFeatureDetector detector;

   	detector.detect(img, keypoints);

   	SiftDescriptorExtractor extractor;

    extractor.compute(img, keypoints, des);

    container.des = des;
    container.keys = keypoints;
}

/* 
*	Function : WriteSiftFile
*	Description : write the keypoints and their descriptor in the sift files (Lowe's binairy format)
*	
*	file : path of the .key file
*	container : container for sift keypoints and their descriptor
*/
void writeSiftFile(const string &file, const struct SFeatures &container)
{
	FILE* f = fopen(file.c_str(), "wb");

    fprintf(f, "%d %d \n", container.des.rows, container.des.cols);

    for(int i = 0; i < container.keys.size(); i++)
    {
      	fprintf(f, "%f %f %f %f \n", container.keys.at(i).pt.y, container.keys.at(i).pt.x, container.keys.at(i).size, (container.keys.at(i).angle*M_PI/180.0));
       	for(int j = 0; j < 128; j++)
       	{
       	fprintf(f, "%d ", (int)container.des.at<float>(i,j));
       	if ((j + 1) % 19 == 0) fprintf(f, "\n");
       	}
       	fprintf(f, "\n");
    }

    fclose(f);
}

/* 
*	Function : sift1Core
*	Description : Find sift points and write those in files
*	
*	dir : directory information
*/
void sift1Core(const util::Directory &dir)
{
	struct SFeatures container;
	string file(dir.getPath());

	cout << endl;

	int n = dir.getNBImages();

	printf("--> Sift searching begins : \n");

	for(int i = 0; i < n; i++)
	{
		file.append(dir.getImage(i));

		doSift(file, container);

		//cout << container.keys.size() << " sift point(s) found in " << dir.getImage(i) << endl;

		while (file[file.size() - 1] != '.')
		{
			file.pop_back();
		}

		file.append("key");

		writeSiftFile(file, container);

		while (file[file.size() - 1] != '/')
		{
			file.pop_back();
		}

		showProgress(i, n, 75, 1);
	}

	showProgress(n, n, 75, 0);

	cout << endl;
}

/* 
*	Function : siftMCore
*	Description : start to find sift point with OpenMPI on the given number of cores
*	
*	path : working directory
*	numcore : number of cores
*/
void siftMCore(const string &path, int numcore)
{
	stringstream c;

	c << "mpirun -n " << numcore << " cDoSift " << path;

	string command = c.str();

	system(command.c_str());
}

/* 
*	Function : siftCMCore
*	Description : start to find sift point with OpenMPI on the given number of cores and on the supercomputer
*	
*	path : working directory
*	numcore : number of cores
*/
void siftMCCore(const string &path, int numcore, int seconds)
{
	stringstream c;

	printf("--> Create the script : \n");

	createSubmit(path, numcore, seconds, 0);

	printf("--> Launch the script : \n");

	c << "msub ulavalSub/submit.sh";

	string command = c.str();

	system(command.c_str());

	cout << "Process launch, you can enter the command \"watch -n 10 showq -u $USER\" to see the progression." << endl << endl;
}


