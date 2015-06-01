HoeffdingTree
=============

Implementation of algorithms for incremental classification [[1](#references),[2](#references)] and regression [[3](#references),[6](#references)] tree learning from time-changing data streams. See [[8](#references)] for detailed description of the main ideas behind these algorithms.

As of this writing this is still work in progress. The plan is to integrate this into [QMiner](https://github.com/qminer/qminer) [9] soon. (Note that this code requires [GLib](https://github.com/qminer/qminer/tree/master/src/glib).)

_Note_: If you'll try to build this, note that GLib uses v0.10 version of libuv; so run `git clone -b v0.10 https://github.com/joyent/libuv.git` in case you need to build it

_Note_: We started integrating this code into QMiner.

_Note_: I will no longer maintain this repository. As of now the learner will be availble through [QMiner](http://qminer.ijs.si/). I _might_ clean up the code so others can use it, but I believe it will be much, much nicer when exposed through QMiner Javascript API. It will be easy to use, assume zero C++ knowledge, require no recompilation, etc. See [QMiner documentation](https://github.com/qminer/qminer/wiki/JavaScript) for details.

_Note_: Check out incremental classification and regression tree learning in QMiner [here](https://github.com/blazs/qminer/blob/master/examples/hoeffdingtree/). User code is in `src/ht.js`. (This is a working version on my fork. For the stable one go [here](https://github.com/qminer/qminer/tree/master/examples/hoeffdingtree).)
## Simple usage example
Configuration file describes data stream. Below is a simple config example.
```
dataFormat: (status,age,sex,survived)
status: discrete(first,second,third,crew)
age: discrete(adult,child)
sex: discrete(male,female)
survived: discrete(yes,no)
```

Future version of the HoeffdingTree --- to be available in QMiner --- will accept JSON configuration.
```js
{
	"dataFormat": ["status", "sex", "age", "survived"],
	"sex": {
		"type": "discrete",
		"values": ["male", "female"]
	},
	"status": {
		"type": "discrete",
		"values": ["first", "second", "third", "crew"]
	},
	"age": {
		"type": "discete",
		"values": ["child", "adult"]
	},
	"survived": {
		"type": "discrete",
		"values": ["yes", "no"]
	}
}
```

The goal is to expose it through QMiner Javascript API. (This will happen the next few days.)

Simple usage example.
```c++
// ... 
int main(int argc, char** argv) {
	// create environment
	Env = TEnv(argc, argv, TNotify::StdNotify);
	// pass some parameters 
	const bool ConceptDriftP = Env.IsArgStr("drift"); // use version that handles concept-drift? 
	const double SplitConfidence = Env.GetIfArgPrefixFlt("-splitConfidence:", 1e-6, "Split confidence"); // 1e-6 
	const double TieBreaking = Env.GetIfArgPrefixFlt("-tieBreaking:", 0.01, "Tie breaking"); // 1e-2 
	const TStr ConfigFNm = Env.GetIfArgPrefixStr("-config:", "titanic.config", "Config file");
	const TStr AttrHeuristic= Env.GetIfArgPrefixStr("-attrEval:", "InfoGain", "Attribute evaluation heuristic");
	const int GracePeriod = Env.GetIfArgPrefixInt("-gracePeriod:", 300, "Grace period"); // 3e2 
	const int DriftCheck = Env.GetIfArgPrefixInt("-driftCheck:", 10000, "Drift check"); // 1e4 
	const int WindowSize = Env.GetIfArgPrefixInt("-windowSize:", 100000, "Window size"); // 1e5 
	
	// usage example 
	PHoeffdingTree ht = THoeffdingTree::New("docs/" + ConfigFNm, GracePeriod, SplitConfidence, TieBreaking,
		DriftCheck, WindowSize);
	ht->SetAdaptive(ConceptDriftP);
	ProcessData("data/titanic-220M.dat", ht);
	ht->Export("exports/titanic-"+TInt(ht->ExportN).GetStr()+".gv", TExportType::DOT);
	return 0;
}
// process data line-by-line 
void ProcessData(const TStr& FileNm, PHoeffdingTree HoeffdingTree) {
	Assert(TFile::Exists(FileNm));
	TFIn FIn(FileNm);
	TStr Line;
	while (FIn.GetNextLn(Line)) { HoeffdingTree->Process(Line, ','); }
}
```

# References
+ [1] Domingos and Hulten, [Mining high-speed data streams](http://homes.cs.washington.edu/~pedrod/papers/kdd00.pdf), KDD`00
+ [2] Hulten et al., [Mining time-changing data streams](http://homes.cs.washington.edu/~pedrod/papers/kdd01b.pdf), KDD`01
+ [3] Ikonomovska et al., [Learning model trees from evolving data streams](http://kt.ijs.si/elena_ikonomovska/DAMI10.pdf), Data Mining and Knowledge Discovery (2011)
+ [4] Gama et al., [On evaluating stream learning algorithms](http://link.springer.com/content/pdf/10.1007%2Fs10994-012-5320-9), Machine Learning (2013)
+ [5] Gama et al., [Issues in Evaluation of Stream Learning Algorithms](http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.154.4848&rep=rep1&type=pdf), KDD`09
+ [6] Elena Ikonomovska, [Algorithms for Learning Regression Trees and Ensembles on Evolving Data Streams](http://kt.ijs.si/elena_ikonomovska/00-disertation.pdf), PhD thesis (2012)
+ [7] Wassily Hoeffding, [Probability Inequalities for Sums of Bounded Random Variables](http://www.csee.umbc.edu/~lomonaco/f08/643/hwk643/Hoeffding.pdf), Journal of the American Statistical Association (1963)
+ [8] Blaz Sovdat, [Algorithms for incremental learning of decision trees from time-changing data streams](http://agava.ijs.si/~blazs/diploma.pdf), Undergraduate thesis (2013)
+ [9] Blaz Fortuna, Jan Rupnik, and others. [QMiner: Data Analytics Platform for Processing Streams of Structured and Unstructured Data](http://sensorlab.ijs.si/files/publications/Fortuna_QMiner_Data_Analytics_Platform.pdf), NIPS`14.
