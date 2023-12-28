//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose:
//
// $NoKeywords: $
//
//=============================================================================//

#include <system_error>
#include <variant>

// Command line arguments.
class Args
{
public:
	// Parse command line and build command line arguments pack. |command_line| Command line.
	[[nodiscard]] static std::variant<Args, std::error_code> FromCommandLine(
			_In_z_ const wchar_t *command_line) noexcept;

	Args( Args &&args ) noexcept
			: values_{args.values_}, count_{args.count_} {
		args.values_ = nullptr;
		args.count_ = 0;
	}

	Args &operator=( Args &&args ) noexcept = delete;

	~Args() noexcept {
		for (int i = 0; i < count_; i++) {
			delete[] values_[i];
		}

		delete[] values_;
		count_ = 0;
	}

	// Args count.
	[[nodiscard]] int count() const noexcept { return count_; }

	// Args values.
	[[nodiscard]] char **values() const noexcept { return values_; }

 private:
	// Args values.
	char **values_;
	// Count of args.
	int count_;

	// Construct args.
	Args( _In_ char **values, _In_ int count ) noexcept
			: values_{values}, count_{count}
	{
	}
};
