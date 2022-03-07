#include <iostream>
#include <coroutine>
#include <type_traits>
#include <utility>
#include <exception>
#include <iterator>
#include <functional>

namespace Lib
{
	template<typename T>
	class [[nodiscard]] generator
	{
	public:
		struct promise_type
		{
		public:
			using value_type = std::remove_reference_t< T >;
			using reference_type = std::conditional_t< std::is_reference_v< T >, T, T& >;
			using pointer_type = value_type*;

			promise_type() = default;
			generator<T> get_return_object() noexcept
			{
				using coroutine_handle = std::coroutine_handle<promise_type>;
				return generator<T>{ coroutine_handle::from_promise(*this) };
			}
			constexpr std::suspend_always initial_suspend() const noexcept { return {}; }
			constexpr std::suspend_always final_suspend() const noexcept { return {}; }
			void unhandled_exception() {}
			void return_void() {}
			std::suspend_always yield_value( std::remove_reference_t<T>&& value ) noexcept
			{
				m_value = std::addressof(value);
				return {};
			}
			// Don't allow any use of 'co_await' inside the generator coroutine.
			template<typename U> std::suspend_never await_transform( U&& value ) = delete;

			reference_type value() const noexcept
			{
				return static_cast< reference_type >( *m_value );
			}

		private:
			pointer_type m_value;
		};

		using coroutine_handle = std::coroutine_handle< promise_type>;

        struct sentinel {};

		class iterator
		{

		public:
			using iterator_category = std::input_iterator_tag;
			using difference_type = std::ptrdiff_t;
			using value_type = typename promise_type::value_type;
			using reference = typename promise_type::reference_type;
			using pointer = typename promise_type::pointer_type;

			// Iterator needs to be default-constructible to satisfy the Range concept.
			explicit iterator( coroutine_handle coroutine = {} ) noexcept
				: m_coroutine( coroutine )
			{}

			friend bool operator==(const iterator& it, sentinel) noexcept { return !it.m_coroutine || it.m_coroutine.done(); }
			friend bool operator!=(const iterator& it, sentinel s) noexcept { return !(it == s); }
			friend bool operator==(sentinel s, const iterator& it) noexcept { return (it == s); }
			friend bool operator!=(sentinel s, const iterator& it) noexcept { return it != s; }

			iterator& operator++()
			{
				m_coroutine.resume();
				return *this;
			}
			void operator++(int)
			{
				(void)operator++();
			}
			reference operator*() const noexcept
			{
				return m_coroutine.promise().value();
			}
			pointer operator->() const noexcept
			{
				return std::addressof(operator*());
			}

		private:
			coroutine_handle m_coroutine;
		};

		generator() = delete;
		generator( const generator& other ) = delete;
		generator( generator&& other ) noexcept
			: m_coroutine(other.m_coroutine)
		{
			other.m_coroutine = nullptr;
		}
		~generator()
		{
			if( m_coroutine )
				m_coroutine.destroy();
		}
		void swap( generator& other ) noexcept
		{
			std::swap( m_coroutine, other.m_coroutine );
		}
		generator& operator=( generator other ) noexcept
		{
			swap( other );
			return *this;
		}

		auto begin()
		{
			if( m_coroutine )
				m_coroutine.resume();
			return iterator{ m_coroutine };
		}
		auto end() noexcept
		{
			return sentinel{};
		}


	private:
		explicit generator(std::coroutine_handle<promise_type> coroutine) noexcept
			: m_coroutine(coroutine)
		{}
		std::coroutine_handle<promise_type> m_coroutine;
	};

	template<typename T> void swap( generator<T>& a, generator<T>& b )
	{
		a.swap(b);
	}

	template<typename FUNC, typename T>
		generator<std::invoke_result_t<FUNC&, typename generator<T>::iterator::reference>> fmap(FUNC func, generator<T> source)
	{
		for (auto&& value : source)
		{
			co_yield std::invoke(func, static_cast<decltype(value)>(value));
		}
	}
}

namespace Test
{
	Lib::generator< int > Range12()
	{
		co_yield 1;
		co_yield 2;
	}

}

int main()
{
	using namespace std;
	for( auto i : Test::Range12() )
		cout << i << endl;
	return 0;
}
