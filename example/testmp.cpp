
int main(int argc)
{	
	int w = 0;
	#pragma omp parallel
	{
		#pragma omp task
		if(argc == 10)
			w = 100;
		else
			w = 20;
	}
	return w;
}