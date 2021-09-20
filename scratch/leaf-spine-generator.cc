#include <iostream>
#include <fstream>
using namespace std;

int spine=0, leaf=0, host=0;
int sbw, sdl, lbw, ldl;

inline int get_host_index(int host_number){
	return host_number;
}

inline int get_leaf_index(int leaf_number){
	return host*leaf + leaf_number;
}

inline int get_spine_index(int spine_number){
	return host*leaf + leaf + spine_number;
}

int main(){

	cout<<"Spine switches [number bandwidth(Gbps) delay(ns)]: ";
	cout.flush(); cin>>spine>>sbw>>sdl;

	cout<<"Leaf switches [number bandwidth(Gbps) delay(ns)]: ";
	cout.flush(); cin>>leaf>>lbw>>ldl;

	cout<<"The number of hosts in each leaf: ";
	cout.flush(); cin>>host;

	string file;
	cout<<"The output file name: ";
	cout.flush(); cin>>file;

	ofstream fout(file.c_str());
	int hosts = host*leaf;
	int nodes = hosts + leaf + spine;
	fout<<nodes<<" "<<leaf+spine << " " << host << " " << leaf << " " <<  spine << " " << hosts+leaf*spine<<endl;//all nodes, all switches, all links

	// ouput all ids of switches
	for (int i = hosts; i < nodes; ++i){
		fout << i << endl;
	}

	for(int i=0;i<hosts;++i){
		int s = get_host_index(i);
		int t = get_leaf_index(i/host);
		fout<<s<<" "<<t<<" "<<lbw<<" "<<ldl<<endl;
	}

	for(int i=0;i<leaf;++i){
		for(int j=0;j<spine;++j){
			int s = get_leaf_index(i);
			int t = get_spine_index(j);
			fout<<s<<" "<<t<<" "<<sbw<<" "<<sdl<<endl;
		}
	}

	fout.close();
	return 0;
}
