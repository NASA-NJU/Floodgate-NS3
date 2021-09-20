#include <iostream>
#include <fstream>
using namespace std;

int pod=0;
int core=0, agg=0, agg_per_pod = 0, edge=0, edge_per_pod = 0, host=0, host_per_pod = 0, host_per_edge = 0;
int bw, dl;

inline int get_host_index(int host_number){
	return host_number;
}

inline int get_edge_index(int edge_number){
	return host + edge_number;
}

inline int get_edge_index(int pod_num, int edge_number){
	return host + pod_num*edge_per_pod + edge_number;
}

inline int get_agg_index(int pod_num, int agg_number){
	return host + edge + pod_num*agg_per_pod + agg_number;
}

inline int get_core_index(int agg_num, int core_number){
	return host + edge + agg + agg_num*(pod-edge_per_pod) + core_number;
}

int main(){

	cout<<"k bandwidth(Gbps) delay(ns): ";
	cout.flush(); cin>>pod>>bw>>dl;

	if (pod%2 != 0){
		cerr << "cannot generate: K must be a multiple of 2 !" << endl;
		return 0;
	}

	host_per_pod = core = (pod/2) * (pod/2);
	host = host_per_pod * pod;
	agg_per_pod = edge_per_pod = pod/2;
	agg = edge = (pod/2) * pod;

	host_per_edge = host/edge;
	int nodes = host + core + agg + edge;

	cout << "The number of pod = " << pod << ", core=" << core
			<< ", aggregation per pod=" << agg_per_pod
			<< ", edge per pod=" << edge_per_pod
			<< ", host per pod=" << host_per_pod
			<< ", host per edge=" << host_per_edge
			<< endl;
	cout << "total switches number:" << core + agg + edge << ", hosts number:" << host << endl;

	string path, file;
	cout << "The output path: ";
	cout.flush(); cin >> path;
	cout << "The output .topo file: ";
	cout.flush(); cin >> file;

	int ouput_switchwin;
	cout << "Output switchwin .cfg configure file (0/1): ";
	cout.flush(); cin >> ouput_switchwin;
	if (ouput_switchwin > 0){
		string switchfile;
		switchfile.assign(path);
		switchfile = switchfile + "/" + file + "-switchwin.cfg";

		ofstream switch_fout(switchfile.c_str());
		switch_fout << core + agg + edge << " 0 0 0" << endl;
		for (int i = host; i < nodes; ++i){
			switch_fout << i << endl;
		}
		switch_fout.close();
	}

	string topofile;
	topofile.assign(path);
	topofile = topofile + "/" + file + ".topo";

	ofstream fout(topofile.c_str());
	fout << nodes << " " << core + agg + edge << " " << host_per_pod << " " << edge << " " << core << " " << host + pod*edge_per_pod*agg_per_pod + agg*(pod-edge_per_pod) << endl; //all nodes, all switches, all links

	// ouput all ids of switches
	for (int i = host; i < nodes; ++i){
		fout << i << endl;
	}

	// host-edge links
	for(int i=0; i < host; ++i){
		int h = get_host_index(i);
		int e = get_edge_index(i/host_per_edge);
		fout << h << " " << e << " " << bw << " " << dl <<endl;
	}

	// edge-agg links
	for (int i = 0; i < pod; ++i){
		for (int j = 0; j < edge_per_pod; ++j){
			for (int k = 0; k < agg_per_pod; ++k){
				int e = get_edge_index(i, j);
				int a = get_agg_index(i, k);
				fout << e << " " << a << " " << bw << " " << dl <<endl;
			}
		}
	}

	// agg-core links
	for (int i = 0; i < pod; ++i){
		for (int j = 0; j < agg_per_pod; ++j){
			for (int k = 0; k < pod - edge_per_pod; ++k){		// note: Aggregation switch has only #pod ports
				int a = get_agg_index(i, j);
				int c = get_core_index(j, k);
				fout << a << " " << c << " " << bw << " " << dl <<endl;
			}
		}
	}

	fout.close();
	return 0;
}



