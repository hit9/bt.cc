// Copyright (c) 2024 Chao Wang <hit9@icloud.com>.
// License: BSD. https://github.com/hit9/Blinker.h

// A lightweight signal/event library for C++, similar to Python's
// blinker, but designed to work with ticking loops.
//
// Requirements: at least C++17.
//
// Mechanism and Concepts
// ~~~~~~~~~~~~~~~~~~~~~~
//
// 1. A signal board contains at most N signals.
//    Where a signal is just composed of an id and name.
// 2. A signal's name should be delimited by dots, e.g. "movement.arrived"
//    Signals are structured into a trie by names in the pre-process stage.
// 3. A subscriber is just a function.
//    It can connect to one or multiple signals by providing signal names,
//    or prefix patterns e.g. "key.*".
// 4. Connections are owned by user, the board doesn't manage them.
// 5. Each connection has a signature, abstracts as a bitset, of which the n'th bit
//    setting to true means that the signal of id n is subscribed by this connection.
// 6. Runtime signal dispatching is done by bitset's AND operation, instead of name matching,
//    so it's fast enough. The string matching is only performed at pre-process stage, to
//    generate signatures for connections.
// 7. Double buffers for a board under the hood:
//    1. The frontend buffer is for subscribers to poll.
//    2. The backend buffer is for new signal emittings.
//    3. The two should be flipped on each tick.
//    4. Each buffer owns a signature of fired signals.
//
// Code Overview
// ~~~~~~~~~~~~~
//
// Pre-process stage:
//
//   1. Creates a Blinker board:
//
//      Blinker::Board<N> board;
//
//   2. Register signals:
//
//      auto a = board.NewSignal("x.a"); // a shared pointer
//
//   3. Connects subscribers:
//
//      auto connection = board.Connect({"x.*"}); // a unique pointer
//
// Runtime stages:
//
//   1. Emits a signal (to the backend buffer):
//
//      a->Emit(123);
//
//   2. In the ticking loop:
//
//      while(true) {
//
//        // Inside your main Update(), poll fired signals from frontend buffer.
//        connection->Poll([&](const Blinker::SignalId id, std::any data) {
//          // executes if any signals fired
//        });
//
//        // Flip double buffers.
//        board.Flip();
//      }

// Version: 0.2.0

#ifndef HIT9_BLINKER_H
#define HIT9_BLINKER_H

#include <any>
#include <bitset>
#include <cassert>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Blinker
{

	//////////////////////////////
	/// Declarations
	//////////////////////////////

	static const std::size_t DefaultNSignal = 1024;

	// SignalId is the type of signal id, starts from 1.
	using SignalId = uint16_t;

	// Signature matches a chunk of signal ids.
	// The n'th bit setting to 1 means the n'th signal is matched.
	template <std::size_t N = DefaultNSignal>
	using Signature = std::bitset<N>;

	// Callback function to be executed on subscribed signal fired.
	using Callback = std::function<void(SignalId, std::any)>;

	// util function to split string into parts by given delimiter.
	static void Split(std::string_view s, std::vector<std::string>& parts, char delimiter);

	// A SignalTrie structures signal ids by names into a tree.
	template <std::size_t N = DefaultNSignal>
	class SignalTrie
	{
	public:
		SignalTrie() {}

		~SignalTrie();

		// Puts a signal id onto this tree by signal name.
		void Put(std::string_view name, SignalId id);

		// Match signals by given pattern, returns a signature of matched signal ids.
		Signature<N> Match(std::string_view pattern) const;

	private:
		// Signature of all signals under this tree.
		// e.g. the node `b` matches all "a.b.*"
		Signature<N> signature;

		// Child tries.
		std::unordered_map<std::string, SignalTrie*> children;

		// If it's a end node of a signal's name, the signal id of which.
		SignalId id = 0;
	};

	template <size_t N = DefaultNSignal>
	class Buffer
	{
	public:
		Buffer() = default;

		// Clears the buffer.
		void Clear();

		// Emits a signal by id and data.
		void Emit(SignalId id, std::any data);

		// Poll fired signals matching given signature.
		int Poll(const Signature<N>& signature, const Callback& cb, SignalId maxId);

	private:
		// A signature stores all ids of fired signals.
		Signature<N> fired;
		// d[j] collects the data for fired signal j.
		std::unordered_map<SignalId, std::vector<std::any>> d;
	};

	class IBoardEmitter
	{
	public:
		// Emits a signal to backend buffer by signal id.
		virtual void Emit(SignalId id, std::any data) = 0;
	};

	template <std::size_t N = DefaultNSignal>
	class IBoardPoller
	{
	public:
		// Poll fired signals matching given signature from frontend buffer.
		virtual int Poll(const Signature<N>& signature, Callback& cb) = 0;
	};

	class Signal
	{
	public:
		Signal(std::string_view name, const SignalId id, IBoardEmitter* board)
			: name(name), id(id), board(board) {}

		std::string_view Name() const { return name; } // cppcheck-suppress returnByReference

		SignalId Id() const { return id; }

		// Emits this signal.
		void Emit(std::any data) { board->Emit(id, data); }

	private:
		std::string	   name;
		const SignalId id;
		// Reference to the board belongs to.
		IBoardEmitter* board;
	};

	template <size_t N>
	class Connection
	{
	public:
		Connection(const Signature<N> signature, IBoardPoller<N>* board)
			: signature(signature), board(board) {}

		// Poll from board's frontend buffer for subscribed signals.
		// If there's some signal fired, the given callback function will be called, and returns a positive count of
		// fired signals.
		int		   Poll(Callback& cb) { return board->Poll(signature, cb); }
		inline int Poll(Callback&& cb) { return board->Poll(signature, cb); }

	private:
		// Signal ids connected.
		const Signature<N> signature;
		// Reference to the board belongs to.
		IBoardPoller<N>* board;
	};

	// The board of signals.
	// Where the N is at least (the max number of signals in this board + 1).
	template <size_t N = DefaultNSignal>
	class Board : public IBoardPoller<N>, public IBoardEmitter
	{
	public:
		Board();

		// Creates a new Signal from this board.
		// Returns nullptr if signal count exceeds N.
		[[nodiscard]] std::shared_ptr<Signal> NewSignal(std::string_view name);

		// Creates a connection to signals matching a single pattern.
		[[nodiscard]] std::unique_ptr<Connection<N>> Connect(const std::string_view pattern);

		// Creates a connection to signals matching given pattern list.
		[[nodiscard]] std::unique_ptr<Connection<N>> Connect(
			const std::initializer_list<std::string_view> patterns);

		[[nodiscard]] std::unique_ptr<Connection<N>> Connect(const std::vector<std::string_view>& patterns);

		// Emits a signal to backend buffer by signal id.
		void Emit(SignalId id, std::any data) override final;

		// Poll fired signals matching given signature from frontend buffer.
		int Poll(const Signature<N>& signature, Callback& cb) override final;

		// Flips the internal double buffers.
		void Flip(void);

		// Clears the internal buffers.
		void Clear(void);

	private:
		// next signal id to use, starts from 1.
		SignalId nextId;
		// Trie of signals ids structured by name.
		SignalTrie<N> tree;
		// Double buffers.
		std::unique_ptr<Buffer<N>> frontend, backend;
	};

	//////////////////////////////
	/// Implementations
	//////////////////////////////

	static void Split(std::string_view s, std::vector<std::string>& parts, char delimiter)
	{
		parts.emplace_back();
		for (auto ch : s)
		{
			ch == '.' ? parts.push_back("")
					  : parts.back().push_back(ch);
		}
	}

	template <std::size_t N>
	SignalTrie<N>::~SignalTrie()
	{
		// free every child recursively
		for (auto p : children)
			delete p.second;
	}

	template <std::size_t N>
	void SignalTrie<N>::Put(std::string_view name, SignalId id)
	{
		std::vector<std::string> parts;
		Split(name, parts, '.');

		// t is the node walked through
		auto t = this;

		for (const auto& p : parts)
		{
			// Creates a node if not exist.
			if (auto [it, inserted] = t->children.try_emplace(p, nullptr); inserted)
				it->second = new SignalTrie();
			// Mark this signal id to its signature.
			t->signature[id] = 1;
			t = t->children[p];
		}

		// The last node.
		t->id = id;
	}

	template <std::size_t N>
	Signature<N> SignalTrie<N>::Match(std::string_view pattern) const
	{
		Signature<N> sig;

		std::vector<std::string> parts;

		Split(pattern, parts, '.');

		auto t = this;

		for (const auto& p : parts)
		{
			// matches all under the subtree
			if (p == "*")
				return t->signature;
			else
			{
				// match by exact name
				// match failure, returns empty signature
				if (t->children.find(p) == t->children.end())
					return sig;
				t = t->children.at(p);
			}
		}

		// The last node, matches a single signal.
		sig[t->id] = 1;
		return sig;
	}

	template <size_t N>
	void Buffer<N>::Clear()
	{
		fired.reset();

		// Only need to clear data for fired signals.
		for (int i = 0; i < N; ++i)
		{
			if (fired[i])
			{
				d[i].clear();
			}
		}
	}

	template <size_t N>
	void Buffer<N>::Emit(SignalId id, std::any data)
	{
		fired[id] = 1, d[id].push_back(data);
	}

	template <size_t N>
	int Buffer<N>::Poll(const Signature<N>& signature, const Callback& cb, SignalId maxId)
	{
		auto match = signature & fired;

		for (int i = 1; i < maxId; i++)
		{
			if (!match[i])
				continue;
			// a signal may emit for multiple times during a frame.
			for (int j = 0; j < d[i].size(); ++j)
				cb(i, d[i][j]);
		}

		return match.count();
	}

	template <size_t N>
	Board<N>::Board()
		: nextId(1), frontend(std::make_unique<Buffer<N>>()), backend(std::make_unique<Buffer<N>>()) {}

	template <size_t N>
	std::shared_ptr<Signal> Board<N>::NewSignal(std::string_view name)
	{
		if (nextId >= N)
		{
			assert(0);
			return nullptr;
		}
		auto id = nextId++;
		tree.Put(name, id);
		return std::make_shared<Signal>(name, id, this);
	}

	template <size_t N>
	std::unique_ptr<Connection<N>>
	Board<N>::Connect(const std::string_view pattern)
	{

		return Connect({ pattern });
	}

	template <size_t N>
	std::unique_ptr<Connection<N>>
	Board<N>::Connect(
		const std::initializer_list<std::string_view> patterns)
	{
		return Connect(std::vector<std::string_view>(patterns));
	}

	template <size_t N>
	std::unique_ptr<Connection<N>> Board<N>::Connect(const std::vector<std::string_view>& patterns)
	{
		Signature<N> signature;
		for (const auto& pattern : patterns)
		{
			signature |= tree.Match(pattern); // cppcheck-suppress useStlAlgorithm
		}
		return std::make_unique<Connection<N>>(signature, this);
	}

	template <size_t N>
	void Board<N>::Emit(SignalId id, std::any data)
	{
		backend->Emit(id, data);
	}

	template <size_t N>
	int Board<N>::Poll(const Signature<N>& signature, Callback& cb)
	{
		return frontend->Poll(signature, cb, nextId);
	}

	template <size_t N>
	void Board<N>::Flip(void)
	{

		frontend->Clear();
		std::swap(frontend, backend);
	}

	template <size_t N>
	void Board<N>::Clear(void)
	{
		frontend->Clear();
		backend->Clear();
	}

} // namespace Blinker

#endif
