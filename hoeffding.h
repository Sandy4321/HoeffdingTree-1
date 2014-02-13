#ifndef HOEFFDING_H
#define HOEFFDING_H

#include <base.h>
#include <mine.h>
#include "parser.h"
#include <map>

#define ISINF(x) _finite(x)
#define ISNAN(x) _isnan(x)

namespace TDatastream {
	///////////////////////////////
	// Forward-Declarations
	class THist;
	class TBin;
	class TAttribute;
	class TNode;
	class TAttrMan;
	ClassHdTP(TExample, PExample);
	ClassHdTP(THoeffdingTree, PHoeffdingTree)
	
	typedef TInt TLabel;
	typedef TVec<THist> THistV;
	typedef TVec<TAttribute> TAttributeV;
	typedef TVec<TBin> TBinV;
	typedef TVec<TNode> TNodeV;
	// typedef TVec<PExample> PExampleV;
	typedef TVec<TAttrMan> TAttrManV;
	// another option is struct { int Idx1, Idx2; double Mx1, Mx2, Diff; };
	typedef TTriple<TPair<TInt, TFlt>, TPair<TInt, TFlt>, TFlt> TBstAttr;
	
	// numeric-attribute-discretization
	const int BinsN = 100;

	// learning task type
	enum class TTaskType : char { CLASSIFICATION, REGRESSION };
	// attribute value type 
	enum class TAttrType : char { DISCRETE, CONTINUOUS };
	// node type 
	enum class TNodeType : char { ROOT, INTERNAL, LEAF };
	// export type 
	enum class TExportType : char { XML, JSON, DOT };
	// attribute heuristic measures 
	enum class TAttrHeuristic : char { INFO_GAIN, GINI_GAIN };

	// Decision-Tree-Exception
	class TDtExcept : public TExcept {
	public:
		static PExcept New(const TStr& MsgStr, const TStr& LocStr = TStr()) {
			return PExcept(new TDtExcept(MsgStr, LocStr));
		}
	private:
		TDtExcept(const TStr& MsgStr, const TStr& LocStr) : TExcept(MsgStr, LocStr) { }
	};

	///////////////////////////////
	// Learning-Algorithm-Parameters
	class TParams {
	public:
		double FadingFactor;
		int WindowSize;
		int DriftCheck;
		int GracePeriod;
		double Delta; // 1-SplitConfidence
		double TieBreaking;
	};

	///////////////////////////////
	// Node-and-Bin-ID-Generator
	ClassTP(TIdGen, PIdGen) //{
	public:
		static PIdGen New() { return new TIdGen(); }
		inline int GetNextLeafId() { return CrrLeafId++; }
		inline int GetNextBinId() { return CrrBinId++; }
	private:
		TIdGen() : CrrLeafId(1), CrrBinId(1) { };
		int CrrLeafId;
		int CrrBinId;
	};

	///////////////////////////////
	// Probability-Estimation-Functions
	class TProbEstimates {
	public:
		// m-estimate; see PhD thesis [Cestnik, 1991] for details
		static double MEstimate(const int& PositivesN, const int& AllN, const double& Apriori, const int& WeightN = 2) {
			Assert(PositivesN >= 0 && PositivesN <= AllN);
			// Let r be the number of positive examples and n be the number of all examples.
			// Furthermore let m be the expert-defined parameter (i.e. ``trust'' parameter) and let p0 be apriori probability.
			// Then we define m-estimate as p := (r+m*p0)/(n+m)
			return (PositivesN+Apriori*WeightN)/(AllN+WeightN); // p = (r+m*p0)/(n+m);
		}
		// laplace estimate; see [Kononenko & Robnik-Sikonja, 2010] for details 
		static double LaplaceEstiamte(const int& PositivesN, const int& NegativesN, const int& OutcomesN) {
			Assert(PositivesN >= 0 && NegativesN >= 0 && OutcomesN >= 0);
			// Let r be the number of positive examples, let n be the number of all examples, and let k be the number of possibles outcomes (i.e. k=2 for coin tossing example)
			// Then we define Laplace probability estimate as p := (r+1)/(n+k). 
			return 1.0*(PositivesN+1)/(NegativesN+PositivesN+OutcomesN); // p = (r+1)/(n+k)
		}
		// relative frequency 
		static double RelativeFreq(const int& PositivesN, const int& AllN) {
			// Assert(PositivesN >= 0 && AllN && AllN > PositivesN);
			return 1.0*PositivesN/AllN;
		}
	};

	///////////////////////////////
	// Helper-functions
	class TMisc {
	public:
		static void AddVec(const int& Scalar, TIntV& FstV, TIntV& SndV); // SndV = Scalar*FstV + SndV
		static double Entropy(const TIntV& FreqV, const int& N); // N = sum(FreqV)
		// Compute variance from sufficient statistic: Sum of squared values, sum of values, and number of values
		inline static double Variance(const double& SqSum, const double& Sum, const int& N) {
			return SqSum/N-TMath::Sqr(Sum/N);
		}
		inline static double StdDev(const double& SqSum, const double& Sum, const int& N) {
			return TMath::Sqrt(Variance(SqSum, Sum, N));
		}
	};
	
	///////////////////////////////
	// Bin (for handling numeric attributes) 
	class TBin {
	public:
		TBin(const double& _Value = 0.0, const int& _Id = 0, const int& _Count = 0)
			: Value(_Value), Id(_Id), Count(_Count), S(0.0), Mean(0.0), T(0.0) { }
		// { printf("INIT = %f\n", Value); getchar(); }
		
		friend bool operator<=(const TBin& Bin1, const TBin& Bin2);
		friend bool operator>=(const TBin& Bin1, const TBin& Bin2);
		friend bool operator<(const TBin& Bin1, const TBin& Bin2);
		friend bool operator>(const TBin& Bin1, const TBin& Bin2);
		friend bool operator==(const TBin& Bin1, const TBin& Bin2);
		friend bool operator!=(const TBin& Bin1, const TBin& Bin2);

		double GetVal() const {
			return Value;
		}
		void Inc(const int& Label) {
			while (Label >= PartitionV.Len()) { PartitionV.Add(0); }
			PartitionV.GetVal(Label)++;
			++Count;
		}
		void Dec(const int& Label) { // NOTE: Asserts serve debugging purposes 
			AssertR(Label < PartitionV.Len(), "Should not happen, by construction.");
			AssertR(PartitionV.GetVal(Label)-- >= 0, "Negative partition count in bin.");
			AssertR(--Count >= 0, "Negative count.");
		}
		// NOTE: Here, ValueV.Len() is the number of examples in the leaf 
		void Inc(const double& RegValue) { // Regression 
			// ValueV.Add(RegValue);
			++Count;
			const double Delta = RegValue - Mean;
			T += RegValue;
			Mean += Delta/Count;
			S += Delta*(RegValue - Mean);
		}
		TBin& operator++() { ++Count; return *this; }
		TBin& operator++(int) { Count++; return *this; }
		double Entropy() const;
		// int operator--() { Assert(Count); return --Count; }
		// int operator--(int) { Assert(Count); return Count--; }
	public:
		TFltV ValueV; // for regression
		double S; // sum of the squares of element and mean differences (x1-mean)^2+(x2-mean)^2+...+(xn-mean)^2
		double Mean; // sample mean 
		double T; // sum of the sample elements x1+x2+...+xn
		double Value; // initialize bin with this value 
		int	Count;
		int Id; // ID needed for forgetting 
		TIntV PartitionV; // for classification 
	};
	
	///////////////////////////////
	// Histogram
	class THist {
	public:
		THist(const int& BinsN_ = BinsN) { } // BinsV.Reserve(BinsN_, BinsN_); } 
		void IncCls(PExample Example, const int& AttrIdx, PIdGen IdGen); // classification
		void DecCls(PExample Example, const int& AttrIdx); // classification 
		void IncReg(PExample Example, const int& AttrIdx); // regression
		// void DecReg(const PExample Example, const int& AttrIdx); // regression 
		double InfoGain(double& SpltVal) const; // classification 
		double GiniGain(double& SpltVal) const; // classification 
		double StdGain(double& SpltVal) const; // regression 
		TBinV BinsV;
		void Print() const;
	};

	///////////////////////////////
	// Attribute-Managment
	class TAttrMan {
	public:
		TAttrMan(const THash<TStr, TInt>& AttrH_ = THash<TStr, TInt>(), const THash<TInt, TStr>& InvAttrH_ = THash<TInt, TStr>(),
			const int& Id_ = -1, const TStr& Nm_ = "Anon", const TAttrType& Type_ = TAttrType::DISCRETE);
		public:
		THash<TStr, TInt> AttrH; // maps attribute value to id 
		THash<TInt, TStr> InvAttrH; // maps id to attribute value, inverting AttrH
		TIntV ValueV; // possible values 
		TAttrType Type; // attribute type 
		TStr Nm; // attribute name 
		TInt Id; // attribute ID used internally 
	};
	
	///////////////////////////////
	// Attribute
	class TAttribute {
	public:
		TAttribute(const int& Id_ = -1, const int& Value_ = -1)
			: Id(Id_), Value(Value_), Num(-1) { }
		TAttribute(const int& Id_, const double& Num_)
			: Id(Id_), Num(Num_), Value(-1) { }
		TAttribute(const TAttribute& Attribute)
			: Id(Attribute.Id), Value(Attribute.Value), Num(Attribute.Num) { }
		// TAttribute& operator=(const TAttribute& Attribute);
		inline bool operator==(const TAttribute& Attr) const {
			return (Value != -1 && Value == Attr.Value) || Num == Attr.Num;
		}
		inline bool operator!=(const TAttribute& Attr) const {
			return !(Attr == *this);
		}
		inline int GetPrimHashCd() const { // NOTE: BAD way of combining HASH CODES!!!
			return Id.GetPrimHashCd()+Value.GetPrimHashCd()+Num.GetPrimHashCd();
		}
		inline int GetSecHashCd() const { // NOTE: BAD way of combining HASH CODES!!!
			return Id.GetSecHashCd()+Value.GetSecHashCd()+Num.GetSecHashCd();
		}
	public:
		TInt Id; // Index in the AttrManV vector 
		TInt Value; // Discrete value; also mapped via hashtable 
		TFlt Num;
	};

	///////////////////////////////
	// Training-Example
	ClassTP(TExample, PExample) // {
	public:
		static PExample New() { return new TExample(); }
		static PExample New(const TAttributeV& AttributesV, const int& Label) {
			return new TExample(AttributesV, Label);
		}
		static PExample New(const TAttributeV& AttributesV, const double& Value) {
			return new TExample(AttributesV, Value);
		}
		TExample() : LeafId(0), BinId(0), Label(-1), Value(0) { }
		TExample(const TAttributeV& AttributesV_, const int& Label_)
			: LeafId(0), BinId(0), AttributesV(AttributesV_), Label(Label_), Value(0) { }
		TExample(const TAttributeV& AttributesV_, const double& Value_)
			: LeafId(0), BinId(0), AttributesV(AttributesV_), Value(Value_), Label(-1) { }
		TExample(const TExample& Example_)
			: LeafId(Example_.LeafId), BinId(Example_.BinId), AttributesV(Example_.AttributesV), Label(Example_.Label), Value(Example_.Value) { }
		TExample& operator=(const TExample& Example);
		inline bool operator<(const TExample& Example) const { return Label < Example.Label; } /* *** */
		inline bool operator==(const TExample& Example) const {
			return LeafId == Example.LeafId && AttributesV == Example.AttributesV && 
				BinId == Example.BinId && Label == Example.Label && Value == Example.Value;
		}
		inline bool operator!=(const TExample& Example) const {
			return !(*this == Example);
		}
		inline void SetLeafId(const int& LeafId_) { LeafId = LeafId_; }
		inline void SetBinId(const int& BinId_) { BinId = BinId_; }
		inline int GetPrimHashCd() const { // NOTE: BAD way of combining HASH CODES!!!
			return AttributesV.GetPrimHashCd()+Label.GetPrimHashCd()+Value.GetPrimHashCd()+LeafId.GetPrimHashCd()+BinId.GetPrimHashCd();
		}
		inline int GetSecHashCd() const { // NOTE: BAD way of combining HASH CODES!!!
			return AttributesV.GetSecHashCd()+Label.GetSecHashCd()+Value.GetSecHashCd()+LeafId.GetSecHashCd()+BinId.GetSecHashCd();
		}
		// TExample(const TStr& Line, const TCh& Separator); // split on Separator and map to integers using hashtables 
	public:
		TInt LeafId;
		TInt BinId;
		TAttributeV AttributesV;
		TLabel Label; // Classification 
		TFlt Value; // Regression only 
	};

	///////////////////////////////
	// Node
	ClassTP(TNode, PNode) // { 
		friend class THoeffdingTree;
	public:
		static PNode New(const int& LabelsN = 2, const TIntV& UsedAttrs = TVec<TInt>(), const int& Id = 0, const TNodeType& Type = TNodeType::LEAF) {
			return new TNode(LabelsN, UsedAttrs, Id, Type);
		}
		static PNode New(const int& LabelsN, const TIntV& UsedAttrV, const TAttrManV& AttrManV, const int& Id, const TNodeType& Type = TNodeType::LEAF) {
			return new TNode(LabelsN, UsedAttrV, AttrManV, Id, Type);
		}
		// TODO: Initialize PartitionV class label distribution counts 
		TNode(const int& LabelsN = 2, const TIntV& UsedAttrs_ = TIntV(), const int& Id_ = 0, const TNodeType& Type_ = TNodeType::LEAF)
			: CndAttrIdx(-1), ExamplesN(0), UsedAttrs(UsedAttrs_), Avg(0), VarSum(0),
			Err(0), TestModeN(0), Id(Id_), Correct(0), All(0), Type(Type_) {
			PartitionV.Reserve(LabelsN, LabelsN);
		}
		TNode(const int& LabelsN, const TIntV& UsedAttrs_, const TAttrManV& AttrManV, const int& Id_, const TNodeType& Type_)
			: CndAttrIdx(-1), ExamplesN(0), UsedAttrs(UsedAttrs_), Avg(0), VarSum(0),
				Err(0), TestModeN(0), Id(Id_), Correct(0), All(0), Type(Type_) {
			PartitionV.Reserve(LabelsN, LabelsN); Init(AttrManV);
		}
		TNode(const TNode& Node);
		~TNode() { Clr(); }
		TNode& operator=(const TNode& Node);
		inline bool operator==(const TNode& Node) const;
		inline bool operator!=(const TNode& Node) const {
			return !(*this == Node);
		}
		double ComputeEntropy() const;
		double ComputeGini() const;
		double InfoGain(const int& AttrIndex, const TVec<TAttrMan>& AttrManV) const; // classification 
		double GiniGain(const int& AttrIndex, const TVec<TAttrMan>& AttrManV) const; // classification 
		double StdGain(const int& AttrIndex, const TVec<TAttrMan>& AttrManV) const; // regression 
		double ComputeTreshold(const double& Delta, const int& LabelsN) const;
		void Split(const int& AttrIndex, const TAttrManV& AttrManV, PIdGen IdGen); // split the leaf on the AttrIndex attribute 
		void Clr(); // forget accumulated examples 
		TBstAttr BestAttr(const TAttrManV& AttrManV, const TTaskType& TaskType = TTaskType::CLASSIFICATION);
		TBstAttr BestRegAttr(const TAttrManV& AttrManV); // regression 
		TBstAttr BestClsAttr(const TAttrManV& AttrManV, const TIntV& BannedAttrV = TVec<TInt>()); // classification 
		void UpdateStats(PExample Example); // regression 
		inline double Std() const {
			// NOTE: Unbiased variance estimator is VarSum/(ExamplesN-1)
			return TMath::Sqrt(VarSum/ExamplesN);
		}
		inline double Mean() const {
			return Avg;
		}
		void UpdateErr(const double& Loss, const double& Alpha);
	// private:
	public:
		void Init(const TAttrManV& AttrManV);
		int CndAttrIdx; // attribute this node tests on; this is set to -1 in leaf nodes 
		int ExamplesN; // count the number of examples we accumulated so far (needed for GracePeriod parameter) 
		double Val; // test for `numerical attribute' <= Val 
		double Avg; // current mean (regression only)
		double VarSum;
		double Err; // current error 
		int TestModeN; // testing mode 
		TNodeType Type;
		TVec<PExample> ExamplesV;
		// TIntV IdxV; // Sacrificed example indices 
#if GLIB_OK
		THash<TExample, TBool> SeenH; // examples sacfrificed for self-evaluation 
#else
		std::map<TExample, bool> seen_h;
#endif
		THash<TTriple<TInt, TInt, TInt>, TInt> Counts; // sufficient statistics; <AttributeID, AttributeValue, Class> 
		TIntV PartitionV; // number of examples with the same label 
		TVec<PNode> ChildrenV;
		TIntV UsedAttrs; // attributes we already used in predecessor nodes 
		THash<TInt, THist> HistH; // for each numeric attribute; maps attribute index to histogram
		// the following are used for time-changeable decision trees 
		TVec<PNode> AltTreesV; // vector of alternate tree root nodes 
		int Id; // monotonically increasing ID, assigned to each node at creation 
		int Correct;
		int All;
	};

	///////////////////////////////
	// Hoeffding-Tree
	ClassTP(THoeffdingTree, PHoeffdingTree) // {
	public:
		THoeffdingTree(const TStr& ConfigNm_, const int& GracePeriod_, const double& SplitConfidence_, const double& TieBreaking_,
			const int& DriftCheck_ = 100, const int& WindowSize_ = 10000, const bool& IsAlt_ = false, const double& FadingFactor_ = 0.9995,
			PIdGen IdGen_ = nullptr)
			: ConfigNm(ConfigNm_), GracePeriod(GracePeriod_), SplitConfidence(SplitConfidence_), TieBreaking(TieBreaking_), DriftExamplesN(0),
			DriftCheck(DriftCheck_), WindowSize(WindowSize_), Params(ConfigNm_), BinsN(1000), MxId(1), IsAlt(IsAlt_), AltTreesN(0), FadingFactor(FadingFactor_),
			CrrErr(1), Salpha(0), Nalpha(0), IdGen(IdGen_), ConceptDriftP(true), ExportN(0) {
				if(IdGen() == nullptr) { IdGen = TIdGen::New(); }
				Init();
		}
		static PHoeffdingTree New(const TStr& ConfigNm, const int& GracePeriod, const double& SplitConfidence, const double& TieBreaking,
			const int& DriftCheck = 100, const int& WindowSize = 10000, const bool& IsAlt = false, const double& FadingFactor = 0.9995, PIdGen IdGen = nullptr) {
			return new THoeffdingTree(ConfigNm, GracePeriod, SplitConfidence, TieBreaking, DriftCheck, WindowSize, IsAlt, FadingFactor, IdGen);
		}
		double Predict(PExample Example) const;
		inline double Predict(const TStr& Line, const TCh& Delimiter = ',') const {
			return Predict(Preprocess(Line, Delimiter));
		}
		TLabel Classify(PNode Node, PExample Example) const;
		TLabel Classify(PExample Example) const;
		inline TLabel Classify(const TStr& Line, const TCh& Delimiter = ',') const {
			return Classify(Preprocess(Line, Delimiter));
		}
		void IncCounts(PNode Node, PExample Example) const;
		void DecCounts(PNode Node, PExample Example) const;
		bool IsAltSplitIdx(PNode Node, const int& AttrIdx) const;
		void CheckSplitValidityCls();
		void ForgetCls(PExample Example) const; // classification 
		void ProcessLeafReg(PNode Leaf, PExample Example); // regression 
		void ProcessLeafCls(PNode Leaf, PExample Example); // classification 
		void SelfEval(PNode Node, PExample Example) const;
		bool TestMode(PNode Node);
		void Process(const TStr& Line, const TCh& Delimiter = ',') {
			Process(Preprocess(Line, Delimiter));
		}
		void Process(PExample Example) {
			if(TaskType == TTaskType::CLASSIFICATION) {
				ProcessCls(Example);
			} else {
				ProcessReg(Example);
			}
		}
		void ProcessCls(PExample Example);
		void ProcessReg(PExample Example);
		PExample Preprocess(const TStr& Line, const TCh& Delimiter = ',') const;
		PNode GetNextNodeCls(PNode Node, PExample Example) const;
		void Clr(PNode Node, PNode SubRoot = nullptr);
		void Export(const TStr& FileNm, const TExportType& ExportType = TExportType::XML) const;
		TLabel NaiveBayes(PNode Node, PExample Example) const;
		inline TLabel Majority(PNode Node) const {
			return Node->PartitionV.GetMxValN();
		}
		inline TStr GetNodeNm(PNode Node) const {
			return AttrManV.GetVal(Node->CndAttrIdx).Nm.CStr();
		}
		inline TStr GetNodeValueNm(PNode Node, const int& ChildN) const {
			Assert(ChildN >= 0);
			return AttrManV.GetVal(Node->CndAttrIdx).InvAttrH.GetDat(ChildN);
		}
		inline TStr GetMajorityNm(PNode Node) const {
			return AttrManV.GetVal(AttrManV.Len()-1).InvAttrH.GetDat(Node->PartitionV.GetMxValN());
		}
		inline bool IsLeaf(PNode Node) const { return Node->CndAttrIdx == -1; }
		void PrintHist(const TStr& FNm, const TCh& Ch = '#') const;
		void Print(PExample Example) const; // print example in human-readable form 
		void SetAdaptive(const bool& DriftP) { ConceptDriftP = DriftP; }
		inline static bool Sacrificed(PNode Node, PExample Example) {
#if GLIB_OK
			return Node->SeenH.IsKey(*Example);
#else
			return Node->seen_h.find(*Example) == Node->seen_h.end();
#endif
		}
		// NOTE: Not implemented yet; avoids using hash tables for self-evaluation 
		//void UpdateIndices(PNode Node) const {
		//	for(int IdxN = 0; IdxN < Node->IdxV.Len(); ++IdxN) {
		//		if(Node->IdxV[IdxN]++ >= WindowSize) { /* throw it out */
		//			Node->IdxV.Del(IdxN); break;
		//		}
		//	}
		//}
	public:
		PNode Root; // root node 
		THash<TStr, TInt> LabelH; // maps label to integer 
		THash<TInt, TStr> InvLabelH; // maps integer to label 
		TVec<THash<TStr, TInt> > AttrsHashV; // vector of attribute hash tables 
		TVec<THash<TInt, TStr> > InvAttrsHashV; // vector of attribute inverse hash tables 
		TAttrManV AttrManV; // attribute managment 
		TParser Params; // data stream parameters 
		int ExportN;
	private:
		double TieBreaking; // tau; when to consider two attributes equally good 
		double SplitConfidence; // delta; NOTE: this is actually error tolerance; condifdence would be 1-`SplitConfidence'
		int GracePeriod; // nmin; recompute heuristic estimates every nmin examples 
		TStr ConfigNm; // configuration file name 
		int DriftCheck; // check for drift every `DriftCheck' examples 
		int WindowSize; // keep `WindowSize' examples in main memory 
		// int MemoryConstraint; // memory contraints (?) 
		const bool IsAlt; // alternate trees are not allowed to grow altenrate trees 
		const int BinsN; // number of bins when apprximating numberic attribute's value distribution with histogram 
		int MxId;
		TTaskType TaskType;
		int AltTreesN; // number of alternate trees 
		TQQueue<PExample> ExampleQ; // last W examples 
		int DriftExamplesN; // examples since last drift check 
		const double FadingFactor; // fading factor used for error estimation; see `On Evaluating Stream Learning Algorithms' [Gama et al., 2013]
		double CrrErr; // current prequential error using fading factors 
		double Salpha;
		double Nalpha;
		PIdGen IdGen; // ID generator 
		bool ConceptDriftP;
	private:
		void Init(); // initialize attribute managment classes 
		void PrintXML(PNode Node, const int& Depth, TFOut& FOut) const; // export decision tree to XML 
		void PrintJSON(PNode Node, const int& Depth, TFOut& FOut) const;
		void PrintDOT(PNode Node, TFOut& FOut, const bool& AlternateP = false) const;
		static void Print(const TCh& Ch = '-', const TInt& Num = 80);
	};
} // namespace TDatastream

#endif
