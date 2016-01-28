
int main(int argc)
{	
	#pragma omp parallel
	{
		#pragma omp task
		if(argc == 10)
			return 100;
		else
			return -1;
	}
}