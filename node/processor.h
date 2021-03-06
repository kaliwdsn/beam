// Copyright 2018 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "../core/radixtree.h"
#include "../utility/dvector.h"
#include "db.h"
#include "txpool.h"

namespace beam {

class NodeProcessor
{
	struct DB
		:public NodeDB
	{
		// NodeDB
		virtual void OnModified() override { get_ParentObj().OnModified(); }
		IMPLEMENT_GET_PARENT_OBJ(NodeProcessor, m_DB)
	} m_DB;

	NodeDB::Transaction m_DbTx;

	UtxoTree m_Utxos;

	size_t m_nSizeUtxoComission;

	void GoUpFast();
	bool GoUpFastInternal();

	bool GoForward(uint64_t, TxBase::Context* pBatch);
	void RollbackTo(Height);
	Height PruneOld();
	Height RaiseFossil(Height);
	Height RaiseTxoLo(Height);
	Height RaiseTxoHi(Height);
	void Vacuum();
	void InitializeUtxos();
	void RequestDataInternal(const Block::SystemState::ID&, uint64_t row, bool bBlock, const NodeDB::StateID& sidTrg);

	bool HandleTreasury(const Blob&);

	bool HandleBlock(const NodeDB::StateID&, TxBase::Context* pBatch);
	bool HandleValidatedTx(TxBase::IReader&&, Height, bool bFwd, const Height* = NULL);
	bool HandleValidatedBlock(TxBase::IReader&&, const Block::BodyBase&, Height, bool bFwd, const Height* = NULL);
	bool HandleBlockElement(const Input&, Height, const Height*, bool bFwd);
	bool HandleBlockElement(const Output&, Height, const Height*, bool bFwd);

	bool ImportMacroBlockInternal(Block::BodyBase::IMacroReader&);
	void RecognizeUtxos(TxBase::IReader&&, Height hMax);

	static void SquashOnce(std::vector<Block::Body>&);
	static uint64_t ProcessKrnMmr(Merkle::Mmr&, TxBase::IReader&&, Height, const Merkle::Hash& idKrn, TxKernel::Ptr* ppRes);

	static const uint32_t s_TxoNakedMin = sizeof(ECC::Point); // minimal output size - commitment
	static const uint32_t s_TxoNakedMax = s_TxoNakedMin + 0x10; // In case the output has the Incubation period - extra size is needed (actually less than this).

	static void TxoToNaked(uint8_t* pBuf, Blob&);
	static bool TxoIsNaked(const Blob&);

	TxoID get_TxosBefore(Height);
	void AdjustOffset(ECC::Scalar&, uint64_t rowid, bool bAdd);

	void InitCursor();
	static void OnCorrupted();
	void get_Definition(Merkle::Hash&, bool bForNextState);
	void get_Definition(Merkle::Hash&, const Merkle::Hash& hvHist);

	typedef std::pair<int64_t, std::pair<int64_t, Difficulty::Raw> > THW; // Time-Height-Work. Time and Height are signed
	Difficulty get_NextDifficulty();
	Timestamp get_MovingMedian();
	void get_MovingMedianEx(uint64_t rowLast, uint32_t nWindow, THW&);

	struct CongestionCache
	{
		struct TipCongestion
			:public boost::intrusive::list_base_hook<>
		{
			Height m_Height;
			bool m_bNeedHdrs;
			std::dvector<uint64_t> m_Rows;

			bool IsContained(const NodeDB::StateID&);
		};

		typedef boost::intrusive::list<TipCongestion> TipList;
		TipList m_lstTips;

		~CongestionCache() { Clear(); }

		void Clear();
		void Delete(TipCongestion*);
		TipCongestion* Find(const NodeDB::StateID&);

	} m_CongestionCache;

	CongestionCache::TipCongestion* EnumCongestionsInternal();

	void DeleteBlocksInRange(const NodeDB::StateID& sidTop, Height hStop);

public:

	struct StartParams {
		bool m_ResetCursor = false;
		bool m_CheckIntegrityAndVacuum = false;
	};

	void Initialize(const char* szPath);
	void Initialize(const char* szPath, const StartParams&);

	virtual ~NodeProcessor();

	struct Horizon {

		Height m_Branching; // branches behind this are pruned
		Height m_SchwarzschildLo; // spent behind this are completely erased
		Height m_SchwarzschildHi; // spent behind this are compacted

		Horizon(); // by default both are disabled.

	} m_Horizon;

	void OnHorizonChanged();

	struct Cursor
	{
		// frequently used data
		NodeDB::StateID m_Sid;
		Block::SystemState::ID m_ID;
		Block::SystemState::Full m_Full;
		Merkle::Hash m_History;
		Merkle::Hash m_HistoryNext;
		Difficulty m_DifficultyNext;

	} m_Cursor;

	struct Extra
	{
		TxoID m_TxosTreasury;
		TxoID m_Txos; // total num of ever created TXOs, including treasury

		Height m_LoHorizon; // lowest accessible height
		Height m_Fossil; // from here and down - no original blocks
		Height m_TxoLo;
		Height m_TxoHi;

	} m_Extra;

	struct SyncData
	{
		NodeDB::StateID m_Target; // can move fwd during sync
		Height m_h0;
		Height m_TxoLo;

	} m_SyncData;

	void LogSyncData();

	// Export compressed history elements. Suitable only for "small" ranges, otherwise may be both time & memory consumng.
	void ExtractBlockWithExtra(Block::Body&, const NodeDB::StateID&);
	void ExportMacroBlock(Block::BodyBase::IMacroWriter&, const HeightRange&);
	void ExportHdrRange(const HeightRange&, Block::SystemState::Sequence::Prefix&, std::vector<Block::SystemState::Sequence::Element>&);
	bool ImportMacroBlock(Block::BodyBase::IMacroReader&);

	struct DataStatus {
		enum Enum {
			Accepted,
			Rejected, // duplicated or irrelevant
			Invalid,
			Unreachable // beyond lo horizon
		};
	};

	bool IsTreasuryHandled() const { return m_Extra.m_TxosTreasury > 0; }

	DataStatus::Enum OnState(const Block::SystemState::Full&, const PeerID&);
	DataStatus::Enum OnBlock(const Block::SystemState::ID&, const Blob& bbP, const Blob& bbE, const PeerID&);
	DataStatus::Enum OnTreasury(const Blob&);

	// use only for data retrieval for peers
	NodeDB& get_DB() { return m_DB; }
	UtxoTree& get_Utxos() { return m_Utxos; }

	Height get_ProofKernel(Merkle::Proof&, TxKernel::Ptr*, const Merkle::Hash& idKrn);

	void CommitDB();

	void EnumCongestions();
	const uint64_t* get_CachedRows(const NodeDB::StateID&, Height nCountExtra); // retval valid till next call to this func, or to EnumCongestions()
	void TryGoUp();

	static bool IsRemoteTipNeeded(const Block::SystemState::Full& sTipRemote, const Block::SystemState::Full& sTipMy);
	bool VerifyBlock(const Block::BodyBase&, TxBase::IReader&&, const HeightRange&);

	virtual void RequestData(const Block::SystemState::ID&, bool bBlock, const PeerID* pPreferredPeer, const NodeDB::StateID& sidTrg) {}
	virtual void OnPeerInsane(const PeerID&) {}
	virtual void OnNewState() {}
	virtual void OnRolledBack() {}
	virtual bool ValidateAndSummarize(TxBase::Context&, const TxBase&, TxBase::IReader&&, bool bBatchReset, bool bBatchFinalize);
	virtual void AdjustFossilEnd(Height&) {}
	virtual bool OpenMacroblock(Block::BodyBase::RW&, const NodeDB::StateID&) { return false; }
	virtual void OnModified() {}

	struct IKeyWalker {
		virtual bool OnKey(Key::IPKdf&, Key::Index) = 0;
	};
	virtual bool EnumViewerKeys(IKeyWalker&) { return true; }

	bool Recover(Key::IDV&, const Output&);

	void RescanOwnedTxos();

	uint64_t FindActiveAtStrict(Height);

	bool ValidateTxContext(const Transaction&); // assuming context-free validation is already performed, but 
	bool ValidateTxWrtHeight(const Transaction&) const;

	struct GeneratedBlock
	{
		Block::SystemState::Full m_Hdr;
		ByteBuffer m_BodyP;
		ByteBuffer m_BodyE;
		Amount m_Fees;
		Block::Body m_Block; // in/out
	};


	struct BlockContext
		:public GeneratedBlock
	{
		TxPool::Fluff& m_TxPool;

		Key::Index m_SubIdx;
		Key::IKdf& m_Coin;
		Key::IPKdf& m_Tag;

		enum Mode {
			Assemble,
			Finalize,
			SinglePass
		};

		Mode m_Mode = Mode::SinglePass;

		BlockContext(TxPool::Fluff& txp, Key::Index, Key::IKdf& coin, Key::IPKdf& tag);
	};

	bool GenerateNewBlock(BlockContext&);
	void DeleteOutdated(TxPool::Fluff&);

	bool GetBlock(const NodeDB::StateID&, ByteBuffer& bbEthernal, ByteBuffer& bbPerishable, Height h0, Height hLo1, Height hHi1);

	struct ITxoWalker
	{
		// override at least one of those
		virtual bool OnTxo(const NodeDB::WalkerTxo&, Height hCreate);
		virtual bool OnTxo(const NodeDB::WalkerTxo&, Height hCreate, Output&);
	};

	bool EnumTxos(ITxoWalker&);
	bool EnumTxos(ITxoWalker&, const HeightRange&);

	struct ITxoRecover
		:public ITxoWalker
	{
		NodeProcessor& m_This;
		ITxoRecover(NodeProcessor& x) :m_This(x) {}

		virtual bool OnTxo(const NodeDB::WalkerTxo&, Height hCreate, Output&) override;
		virtual bool OnTxo(const NodeDB::WalkerTxo&, Height hCreate, Output&, const Key::IDV&) = 0;
	};

	struct ITxoWalker_UnspentNaked
		:public ITxoWalker
	{
		virtual bool OnTxo(const NodeDB::WalkerTxo&, Height hCreate) override;
	};

#pragma pack (push, 1)
	struct UtxoEvent
	{
		typedef ECC::Point Key;
		static_assert(sizeof(Key) == sizeof(ECC::uintBig) + 1, "");

		struct Value {
			ECC::Key::IDV::Packed m_Kidv;
			uintBigFor<Height>::Type m_Maturity;
			AssetID m_AssetID;
			uint8_t m_Added;
		};
	};
#pragma pack (pop)

	virtual void OnUtxoEvent(const UtxoEvent::Value&) {}
	virtual void OnDummy(const Key::ID&, Height) {}

	static bool IsDummy(const Key::IDV&);

private:
	size_t GenerateNewBlockInternal(BlockContext&);
	void GenerateNewHdr(BlockContext&);
	DataStatus::Enum OnStateInternal(const Block::SystemState::Full&, Block::SystemState::ID&);
};



} // namespace beam
