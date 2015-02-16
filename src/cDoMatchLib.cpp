/*
*	File : cDoMatchLib.cpp
*	Author : Émile Robitaille @ LERobot
*	Creation date : 07/31/2014
*	Version : 1.0
*	
*	Description : Functions to make match in parallel
*/


#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/features2d/features2d.hpp>
#include <opencv2/flann/flann.hpp>
#include <mpi.h>
#if CV_VERSION_MAJOR == 2
#include <opencv2/nonfree/nonfree.hpp>
#include <opencv2/nonfree/features2d.hpp>
#elif CV_VERSION_MAJOR == 3
#include <opencv2/xfeatures2d.hpp>
#include <opencv2/xfeatures2d/nonfree.hpp>
#endif

#include "directory.h"
#include "util.h"
#include "dosift.h"
#include "domatch.h"
#include "dogeometry.h"
#include "cDoMatchLib.h"

using namespace std;
using namespace cv;
#if CV_VERSION_MAJOR == 3
using namespace xfeatures2d;
#endif



/* 
*	Function : boss
*	Description : code for the boss
*	
*	numcore : number of cores
*	dir : directory information
*/
int* boss(int numcore, const util::Directory &dir)
{
	int* dis = createDist4Match(dir.getNBImages(), numcore);

	return dis;
}


/* 
*	Function : serializeVector
*	Description : to serialize a vector
*	
*	sender : vector to serialize
*	img1 : images 1 index
*	img2 : images 2 index
*/
float* serializeContainer(const struct Matchespp &container)
{
	float* serialTab;

	int s = 15 + container.NM * 2;

	serialTab = (float*) malloc(s * sizeof(float));

	serialTab[0] = (float) s;
	serialTab[1] = (float) container.idx[0];
	serialTab[2] = (float) container.idx[1];
	serialTab[3] = (float) container.NM;

	for(int i = 0; i < container.NM * 2; i+=2)
	{
		int idx = i + 4;
		int idx2 = i/2;
		serialTab[idx] = (float) container.matches[idx2].queryIdx;
		serialTab[idx+1] = (float) container.matches[idx2].trainIdx;
	}

	int seek = 4 + container.NM * 2;

	serialTab[seek] = (float) container.NI;

	const double* M = container.H.ptr<double>();

	serialTab[seek+1] = (float) M[0];
	serialTab[seek+2] = (float) M[1];
	serialTab[seek+3] = (float) M[2];
	serialTab[seek+4] = (float) M[3];
	serialTab[seek+5] = (float) M[4];
	serialTab[seek+6] = (float) M[5];
	serialTab[seek+7] = (float) M[6];
	serialTab[seek+8] = (float) M[7];
	serialTab[seek+9] = (float) M[8];

	serialTab[seek+10] = container.ratio;

	return serialTab;
}


/* 
*	Function : endComm
*	Description : implementation to end a conversation
*	
*	sender : sender (worker) ID
*/
void endComm(int sender)
{
	float endSignal = -1.0;
	MPI_Send(&endSignal, 1, MPI_FLOAT, sender, 0, MPI_COMM_WORLD);
} 


/* 
*	Function : worker
*	Description : code for the workers
*	
*	dir : directory information
*	recv : relative information about distribution
*	geo : if to do geometry or not
*/
void worker(const util::Directory &dir, int* recv, int geo)
{
	int aim = recv[0];
	int end = recv[1];

	int netID, seek = 0, compute = 0, stop = 0;

	MPI_Comm_rank(MPI_COMM_WORLD, &netID);

	vector<string> list;

	listDir(dir, list);

	struct SFeatures keys1, keys2;

	float* serialMatch;		 

	for(int i = 0; !stop; i++)
	{
		if(compute)
		{
			readAndAdjustSiftFile(dir.getPath(), dir.getImage(i), list[i], keys1);
		}

		for (int j = 0; j < i; j++)
		{

			if(seek == aim){readAndAdjustSiftFile(dir.getPath(), dir.getImage(i), list[i], keys1);compute=1;}

			if(compute)
			{
				struct Matchespp container(j, i);

				readAndAdjustSiftFile(dir.getPath(), dir.getImage(j), list[j], keys2);

				doMatch(keys1, keys2, container, geo);

				serialMatch = serializeContainer(container);

				//cout << "[CORE " << netID << "]: " << container.NM << " match(es) found between " << dir.getImage(j) << " and " << dir.getImage(i) << endl;

				container.reset();

				MPI_Send(serialMatch, serialMatch[0], MPI_FLOAT, SECRETARY, 1, MPI_COMM_WORLD);

				free(serialMatch);
			}

			seek++;
			if(seek == end){compute=0;stop=1;}
		}
	}

	endComm(SECRETARY);
}


/* 
*	Function : writeSerialMatch
*	Description : code to write in the file from a serial matches structure, without geo
*	
*	f : file descriptor
*	container : contains matches information
*	n : number of images
*	bar : to print bar or not
*/
void writeSerialMatch(const string &path, const vector<float*> &container, int n, int bar)
{
	string file1(path);

	file1.append("matches.init.txt");

	FILE *f1 = fopen(file1.c_str(), "wb");

	int ni = ( 1 + sqrt( 1 + 8 * n ) ) / 2;

	for (int i = 0; i < ni; i++)
	{	
		for (int j = 0; j < ni; j++)
		{
			int reverse;
			float* pter = searchIDX(i, j, container, &reverse);

			if (pter != NULL && !reverse)
			{
				int NM = (int) pter[3];

				fprintf(f1, "%d %d\n", (int) pter[1], (int) pter[2]);
				fprintf(f1, "%d\n", NM);

				int num = 4;

				for(int j = 0; j < NM; j++)
				{
					fprintf(f1, "%d %d\n", (int) pter[num], (int) pter[num + 1]);
					num += 2;
				}
			}
			else if (pter != NULL && reverse)
			{
				int NM = (int) pter[3];

				fprintf(f1, "%d %d\n", (int) pter[2], (int) pter[1]);
				fprintf(f1, "%d\n", NM);

				int num = 4;

				for(int j = 0; j < NM; j++)
				{
					fprintf(f1, "%d %d\n", (int) pter[num + 1], (int) pter[num]);
					num += 2;
				}
			}

			if (bar) showProgress(i * ni + j, ni * ni, 75, 1);
		}
	}

	if (bar) showProgress(ni * ni, ni * ni, 75, 0);

	fclose(f1);
}

/* 
*	Function : searchIDX
*	Description : code to search the tab idx of a certain i and j match
*	
*	i : first image
*	j : second image
*	container : vector of matches informations
*	reverse : indicate if found but in the reverse order
*
*	return : the serial information if found, else NULL
*/
float* searchIDX(int i, int j, const vector<float*> &container, int* reverse)
{
	int num = container.size();

	*reverse = 0;

	for(int a = 0; a < num; a++)
	{
		if((int) container[a][1] == i && (int) container[a][2] == j)
			return container[a];
		if((int) container[a][1] == j && (int) container[a][2] == i)
		{
			*reverse = 1;
			return container[a];
		}
	}

	return NULL;
}


/* 
*	Function : writeSerialMatchespp
*	Description : code to write in the file from a serial matchespp structure
*	
*	path : directory path
*	container : contains the matches information
*	n : number of images
*	bar : to print bar or not
*/
void writeSerialMatchespp(const string &path, const vector<float*> &container, int n, int bar)
{
	string file1(path);
	string file2(path);

	file1.append("matches.init.txt");
	file2.append("ulavalSFM.txt");

	FILE *f1 = fopen(file1.c_str(), "wb");

	FILE *f2 = fopen(file2.c_str(), "wb");

	int ni = ( 1 + sqrt( 1 + 8 * n ) ) / 2;

	for (int i = 0; i < ni; i++)
	{	
		for (int j = 0; j < ni; j++)
		{
			int reverse;
			float* pter = searchIDX(i, j, container, &reverse);

			if (pter != NULL && !reverse)
			{
				int NM = (int) pter[3];

				fprintf(f1, "%d %d\n", (int) pter[1], (int) pter[2]);
				fprintf(f1, "%d\n", NM);

				int num = 4;

				for(int j = 0; j < NM; j++)
				{
					fprintf(f1, "%d %d\n", (int) pter[num], (int) pter[num + 1]);
					num += 2;
				}

				if(pter[num] > 0)
				{
					fprintf(f2, "%d %d\n", (int) pter[1], (int) pter[2]);

				    fprintf(f2, "%d\n", (int) pter[num]);
				    fprintf(f2, "%f\n", pter[num + 10]);

				    fprintf(f2, "%f %f %f %f %f %f %f %f %f\n", pter[num + 1], pter[num + 2], 
				     	pter[num + 3], pter[num + 4], pter[num + 5], pter[num + 6], 
				       	pter[num + 7], pter[num + 8], pter[num + 9]);
				}
			}
			else if (pter != NULL && reverse)
			{
				int NM = (int) pter[3];

				fprintf(f1, "%d %d\n", (int) pter[2], (int) pter[1]);
				fprintf(f1, "%d\n", NM);

				int num = 4;

				for(int j = 0; j < NM; j++)
				{
					fprintf(f1, "%d %d\n", (int) pter[num + 1], (int) pter[num]);
					num += 2;
				}

				if(pter[num] > 0)
				{
					fprintf(f2, "%d %d\n", (int) pter[2], (int) pter[1]);

			        fprintf(f2, "%d\n", 0);
			        fprintf(f2, "%f\n", 0.0);

			        fprintf(f2, "%f %f %f %f %f %f %f %f %f\n", pter[num + 1], pter[num + 4], 
			        	pter[num + 7], pter[num + 2], pter[num + 5], pter[num + 8], 
			        	pter[num + 3], pter[num + 6], pter[num + 9]);
				}
			}

			if (bar) showProgress(i * ni + j, ni * ni, 75, 1);
		}
	}

	if (bar) showProgress(ni * ni, ni * ni, 75, 0);

	fclose(f1);
	fclose(f2);
}

/* 
*	Function : recvFromWorker
*	Description : recv implementation for the secretary
*/
float* recvFromWorker(vector<int> &list)
{
	MPI_Status status;
	float* serialMatch; 
	float garbage;
	int sender, s;

	MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

	int tag = status.MPI_TAG;
	sender = status.MPI_SOURCE;

	list.push_back(sender);

	if(tag > 0)
	{
		MPI_Get_count(&status, MPI_FLOAT, &s);

		serialMatch = (float*) malloc(s * sizeof(float));
		MPI_Recv(serialMatch, s, MPI_FLOAT, sender, 1, MPI_COMM_WORLD, &status);
	}
	else
	{
		MPI_Recv(&garbage, 1, MPI_FLOAT, sender, tag, MPI_COMM_WORLD, &status);
		serialMatch = NULL;
	}

	return serialMatch; 
}

/* 
*	Function : secretary
*	Description : code for the secretary
*	
*	path : directory path
*	numcore : number of cores
*	n : number of images
*	bar : if to print the bar or not
*	geo : if to print geo information or not
*/
void secretary(const string &path, int numcore, int n, int bar, int geo)
{
	vector<float*> v_serialMatch;
	vector<int> list;

	printf("GEO : %d\n", geo);

	int end = 1, i = 0;

	string file(path);
	file.append(MATCHFILE);

	printf("--> Matching : \n");

	int tbar = n + numcore - 1;

	while(end < numcore)
	{
		float* serialMatch;

		serialMatch = recvFromWorker(list);
		if (serialMatch)
			v_serialMatch.push_back(serialMatch);
		else
			end++;
		i++;
		if (bar) showProgress(i, tbar, 75, 1);
	}

	if (bar) showProgress(n, tbar, 75, 0);

	cout << "--> Writing file : " << endl;

	if (geo)
		writeSerialMatchespp(path, v_serialMatch, n, bar);
	else
		writeSerialMatch(path, v_serialMatch, n, bar);

	cout << endl;
}
