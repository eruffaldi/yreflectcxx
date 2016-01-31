//#include <array>

namespace www
{
	struct Pippo
	{
		int data[20];
		enum class PippoEnum: int { A,B,C};
		PippoEnum e;

		struct Pippo2
		{
				int w;
		};
	};

	template <class T, int w = 20>
	struct X_
	{
		T A = 0;
		T B[w];
	};

	template <>
	struct X_<float,150>
	{
		typedef float T;
		T A = 0;
		T B[150];
		T C;
	};

	typedef X_<int,10> X;

	template <int k>
	using Xi = X_<double,k>; // unsupported

	using X = X_<int,10>; // int seems invisible (?)

	using X2 = X_<float,150>; // int seems invisible (?)

	using X3 = Xi<20>; // 


}

/*
 Prev:
 3 while: 1, 10 continue is invisible OK BUT 10 twice!
 5 if: 3
 7 expr: 5
 10 if: 5
 12 expr: 10
 13 return: 7 3
 */
int main(int argc, char const *argv[]) // 1
{ // 2 removed from flow? 
	www::X q;
	www::X2 q2;
	int w = argc;
	while(w > 0) // 3
	{ // 4 -> removed from flow
		if(w == 2) // 5
		{ // 6 -> removed from flow
			argc++; // 7
			break; // 8
		}
		else 
		{ // 9 -> removed from flow
			if(argc > 2) // 10
				continue; // 11
			else 
				w--; // 12
		}
	}	
	return sizeof(www::X2) + q.A; // 13
}

www::X3 q2;
