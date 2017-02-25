#include <stdio.h>
#include <gtest/gtest.h>

//-------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	testing::InitGoogleTest(&argc, argv);
	srand((uint32_t)::time(0));
	
	return RUN_ALL_TESTS();
}
