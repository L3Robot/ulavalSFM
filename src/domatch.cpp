/*
*	File : cDoMatch.cpp
*	Author : Émile Robitaille @ LERobot
*	Creation date : 07/08/2014
*	Version : 1.0
*	
*	Description : Program to make match in parallel
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
#include <mpi.h>

#include "directory.h"
#include "util.h"
#include "cDoMatchLib.h"

using namespace std;
using namespace cv;


int main(int argc, char** argv)
{
	MPI_Init(&argc, &argv);

	int* dist;
	int recv[2];
	int bar = 1;
	int geo = 1;

	//quick parsing of bar printing or not and geometric
	if(argc > 2)
	{
		if(argv[2][0] == '0')
			bar = 0;
		if(argv[3][0] == '0')
			geo = 0;
	}

	double the_time;

	int netSize;
	int netID;

	MPI_Comm_size(MPI_COMM_WORLD, &netSize);
	MPI_Comm_rank(MPI_COMM_WORLD, &netID);

	util::Directory dir(argv[1]);

	if(netSize < 2)
	{
		if(netID == 0)
		{
			printf("[ERROR] At most 2 cores are needed\n");
		}
		exit(1);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	if(netID == 0)
	{
		the_time = MPI_Wtime();
	}

	MPI_Barrier(MPI_COMM_WORLD);

	if(netID == 0)
	{
		dist = boss(netSize, dir);
	}

	MPI_Scatter(dist, 2, MPI_INT, recv, 2, MPI_INT, 0, MPI_COMM_WORLD);

	if(netID == 0)
	{
		int n = dir.getNBImages() * (dir.getNBImages() - 1) / 2;
		deleteDist(dist);
		secretary(dir.getPath(), netSize, n, bar, geo);
	}
	else
	{
		worker(dir, recv, geo);
	}

	MPI_Barrier(MPI_COMM_WORLD);

	if(netID == 0)
	{
		printf("The program takes approximately %f second(s)\n", MPI_Wtime() - the_time);
	}

	MPI_Finalize();

	return 0;
}












