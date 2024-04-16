// Copyright (c) 2024 Chao Wang <hit9@icloud.com>.
// License: BSD. https://github.com/hit9/blinker.h

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
//   1. Creates a blinker board:
//
//      blinker::Board<N> board;
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
//        connection->Poll([&](const blinker::SignalId id, std::any data) {
//          // executes if any signals fired
//        });
//
//        // Flip double buffers.
//        board.Flip();
//      }

// Version: 0.1.0

#ifndef HIT9_BLINKER_H
#define HIT9_BLINKER_H

#include <any>
#include <bitset>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace blinker {

// SignalId is the type of signal id, starts from 1.
using SignalId = uint16_t;

// Signature matches a chunk of signal ids.
// The n'th bit setting to 1 means the n'th signal is matched.
template <std::size_t N>
using Signature = std::bitset<N>;

// Callback function to be executed on subscribed signal fired.
using Callback = std::function<void(SignalId, std::any)>;

// util function to split string into parts by given delimiter.
static void split(std::string_view s, std::vector<std::string>& parts, char delimiter) {
  parts.emplace_back();
  for (auto ch : s) ch == '.' ? parts.push_back("") : parts.back().push_back(ch);
};

// A SignalTrie structures signal ids by names into a tree.
template <std::size_t N>
class SignalTrie {
 private:
  // Signature of all signals under this tree.
  // e.g. the node `b` matches all "a.b.*"
  Signature<N> signature;
  // Child tries.
  std::unordered_map<std::string, SignalTrie*> children;
  // If it's a end node of a signal's name, the signal id of which.
  SignalId id = 0;

 public:
  SignalTrie() {}
  ~SignalTrie() {  // free every child recursively
    for (auto p : children) delete p.second;
  }
  // Puts a signal id onto this tree by signal name.
  void Put(std::string_view name, SignalId id) {
    std::vector<std::string> parts;
    split(name, parts, '.');
    auto t = this;  // t is the node walked through
    for (const auto& p : parts) {
      // Creates a node if not exist.
      if (t->children.find(p) == t->children.end()) t->children[p] = new SignalTrie();
      // Mark this signal id to its signature.
      t->signature[id] = 1;
      t = t->children[p];
    }
    // The last node.
    t->id = id;
  }
  // Match signals by given pattern, returns a signature of matched signal ids.
  Signature<N> Match(std::string_view pattern) const {
    Signature<N> signature;
    std::vector<std::string> parts;
    split(pattern, parts, '.');
    auto t = this;
    for (const auto& p : parts) {
      // matches all under the subtree
      if (p == "*")
        return t->signature;
      else {  // match by exact name
        // match failure, returns empty signature
        if (t->children.find(p) == t->children.end()) return signature;
        t = t->children.at(p);
      }
    }
    // The last node, matches a single signal.
    signature[t->id] = 1;
    return signature;
  }
};

template <size_t N>
class Buffer {
 private:
  // A signature stores all ids of fired signals.
  Signature<N> fired;
  // d[j] is the data for fired signal j.
  std::any d[N];

 public:
  Buffer() = default;
  // Clears the buffer.
  void Clear() { fired.reset(); }

  // Emits a signal by id and data.
  void Emit(SignalId id, std::any data) { fired[id] = 1, d[id] = data; }

  // Poll fired signals matching given signature.
  int Poll(Signature<N> signature, Callback cb, SignalId maxId) {
    auto match = signature & fired;
    for (int i = 1; i < maxId; i++)
      if (match[i]) cb(i, d[i]);
    return match.count();
  }
};

// Forward declarations.
template <size_t N>
class Signal;

template <size_t N>
class Connection;

template <size_t N = 1024>
class Board {
 private:
  // next signal id to use, starts from 1.
  SignalId nextId;
  // Trie of signals ids structured by name.
  SignalTrie<N> tree;
  // Double buffers.
  std::unique_ptr<Buffer<N>> frontend, backend;

 public:
  Board() : nextId(1), frontend(std::make_unique<Buffer<N>>()), backend(std::make_unique<Buffer<N>>()) {}

  // Creates a new Signal from this board.
  // Returns nullptr if signal count exceeds N.
  std::shared_ptr<Signal<N>> NewSignal(std::string_view name) {
    if (nextId > N) return nullptr;
    auto id = nextId++;
    tree.Put(name, id);
    return std::make_shared<Signal<N>>(name, id, *this);
  }
  // Creates a connection to signals matching a single pattern.
  std::unique_ptr<Connection<N>> Connect(const std::string_view pattern) { return Connect({pattern}); }
  // Creates a connection to signals matching given pattern list.
  std::unique_ptr<Connection<N>> Connect(const std::initializer_list<std::string_view> patterns) {
    Signature<N> signature;
    for (auto& pattern : patterns) signature |= tree.Match(pattern);
    return std::make_unique<Connection<N>>(signature, *this);
  }
  // Emits a signal to backend buffer by signal id.
  void Emit(SignalId id, std::any data) { backend->Emit(id, data); }
  // Poll fired signals matching given signature from frontend buffer.
  int Poll(Signature<N> signature, Callback cb) { return frontend->Poll(signature, cb, nextId); }
  // Flips the internal double buffers.
  void Flip(void) {
    frontend->Clear();
    std::swap(frontend, backend);
  }
};

template <size_t N>
class Signal {
 private:
  std::string_view name;
  const SignalId id;
  // Reference to the board belongs to.
  Board<N>& board;

 public:
  Signal(std::string_view name, const SignalId id, Board<N>& board) : name(name), id(id), board(board) {}
  std::string_view Name() const { return name; }
  SignalId Id() const { return id; }
  // Emits this signal.
  void Emit(std::any data) { board.Emit(id, data); }
};

template <size_t N>
class Connection {
 private:
  // Signal ids connected.
  const Signature<N> signature;
  // Reference to the board belongs to.
  Board<N>& board;

 public:
  Connection(const Signature<N> signature, Board<N>& board) : signature(signature), board(board) {}
  // Poll from board's frontend buffer for subscribed signals.
  // If there's some signal fired, the given callback function will be called, and returns a positive count of
  // fired signals.
  int Poll(Callback cb) { return board.Poll(signature, cb); }
};

}  // namespace blinker

#endif
