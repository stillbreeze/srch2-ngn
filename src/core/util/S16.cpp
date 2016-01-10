#include "util/cowvector/compression/S16.h"

const unsigned S16::big_directory[][S16::MAX_COL] =
{
	{7 , 7 , 7, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{8 , 8 , 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{10, 9 , 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{28, 0 , 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};

const unsigned S16::small_directory[][S16::MAX_COL] =
{
	{2 , 2 , 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2},
	{4 , 3 , 3, 3, 3, 3, 3, 3, 3, 0, 0, 0, 0, 0},
	{4 , 4 , 4, 4, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0},
	{4 , 4 , 4, 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0},
	{5 , 5 , 5, 5, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0},
	{6 , 5 , 5, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0},
	{6 , 6 , 4, 4, 4, 4, 0, 0, 0, 0, 0, 0, 0, 0},
	{6 , 6 , 6, 5, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{6 , 6 , 6, 6, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{7 , 7 , 7, 7, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{8 , 7 , 7, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{8 , 8 , 6, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{10, 9 , 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{14, 14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{15, 13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{28, 0 , 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}
};
