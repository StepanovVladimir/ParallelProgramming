#pragma once

#include <windows.h>
#include <stdexcept>

template<typename T>
class LogBuffer
{
	class Node
	{
	public:
		Node(Node* prev, Node* next)
			: prev(prev)
			, next(next)
		{
		}
		virtual ~Node() = default;
		virtual T& GetData()
		{
			throw std::runtime_error("Cannot dereference end iterator");
		}

		Node* prev;
		Node* next;
	};

public:
	LogBuffer()
		: _size(0)
	{
		_first = new Node(nullptr, nullptr);
		_last = _first;
	}

	LogBuffer(const LogBuffer& other)
	{
		LogBuffer tmp;
		for (T data : other)
		{
			tmp.Log(data);
		}
		_first = new Node(nullptr, nullptr);
		_last = _first;
		*this = move(tmp);
	}

	LogBuffer(LogBuffer&& other)
	{
		Node* endNode = new Node(nullptr, nullptr);
		_first = other._first;
		_last = other._last;
		_size = other._size;
		other._first = endNode;
		other._last = endNode;
		other._size = 0;
	}

	~LogBuffer()
	{
		Clear();
		delete _last;
	}

	LogBuffer& operator=(const LogBuffer& other)
	{
		if (this != &other)
		{
			LogBuffer tmp(other);
			*this = move(tmp);
		}
		return *this;
	}

	LogBuffer& operator=(LogBuffer&& other)
	{
		if (this != &other)
		{
			Clear();
			std::swap(_first, other._first);
			std::swap(_last, other._last);
			std::swap(_size, other._size);
		}
		return *this;
	}

	void AddCriticalSection(CRITICAL_SECTION* criticalSection)
	{
		_criticalSection = criticalSection;
	}

	void Log(const T& data)
	{
		if (_criticalSection != nullptr)
		{
			EnterCriticalSection(_criticalSection);
		}

		Node* newNode = new NodeWithData(data, _last->prev, _last);
		if (IsEmpty())
		{
			_first = newNode;
		}
		else
		{
			_last->prev->next = newNode;
		}
		_last->prev = newNode;
		_size++;

		if (_criticalSection != nullptr)
		{
			LeaveCriticalSection(_criticalSection);
		}
	}

	size_t GetSize() const
	{
		return _size;
	}

	bool IsEmpty() const
	{
		return _size == 0;
	}

	void Clear()
	{
		Node* curNode = _first;
		while (curNode->next != nullptr)
		{
			curNode = curNode->next;
			delete curNode->prev;
		}
		_first = _last;
		_size = 0;
	}

	class CIterator
	{
	public:
		CIterator()
			: _node(nullptr)
			, _isReverse(false)
		{
		}

		CIterator& operator++()
		{
			!_isReverse ? DoIncrement() : DoDecrement();
			return *this;
		}

		const CIterator operator++(int)
		{
			CIterator tmp(*this);
			++(*this);
			return tmp;
		}

		CIterator& operator--()
		{
			!_isReverse ? DoDecrement() : DoIncrement();
			return *this;
		}

		const CIterator operator--(int)
		{
			CIterator tmp(*this);
			--(*this);
			return tmp;
		}

		T& operator*() const
		{
			if (_node == nullptr)
			{
				throw std::runtime_error("Cannot dereference empty iterator");
			}
			return _node->GetData();
		}

		bool operator==(const CIterator& other) const
		{
			return _node == other._node;
		}

		bool operator!=(const CIterator& other) const
		{
			return _node != other._node;
		}

	private:
		friend LogBuffer;

		Node* _node;
		bool _isReverse;

		CIterator(Node* node, bool isReverse)
			: _node(node)
			, _isReverse(isReverse)
		{
		}

		void DoIncrement()
		{
			if (_node == nullptr)
			{
				throw std::runtime_error("Cannot increment empty iterator");
			}
			if (_node->next == nullptr)
			{
				throw std::runtime_error("Cannot increment end iterator");
			}
			_node = _node->next;
		}

		void DoDecrement()
		{
			if (_node == nullptr)
			{
				throw std::runtime_error("Cannot decrement empty iterator");
			}
			if (_node->prev == nullptr)
			{
				throw std::runtime_error("Cannot decrement begin iterator");
			}
			_node = _node->prev;
		}
	};

	CIterator begin() const
	{
		return CIterator(_first, false);
	}

	CIterator end() const
	{
		return CIterator(_last, false);
	}

	const CIterator cbegin() const
	{
		return CIterator(_first, false);
	}

	const CIterator cend() const
	{
		return CIterator(_last, false);
	}

	CIterator rbegin() const
	{
		return CIterator(_first, true);
	}

	CIterator rend() const
	{
		return CIterator(_last, true);
	}

	const CIterator crbegin() const
	{
		return CIterator(_first, true);
	}

	const CIterator crend() const
	{
		return CIterator(_last, true);
	}

private:
	class NodeWithData : public Node
	{
	public:
		NodeWithData(const T& data, Node* prev, Node* next)
			: _data(data)
			, Node(prev, next)
		{
		}

		T& GetData() override
		{
			return _data;
		}

	private:
		T _data;
	};

	Node* _first;
	Node* _last;
	size_t _size;
	CRITICAL_SECTION* _criticalSection = nullptr;
};