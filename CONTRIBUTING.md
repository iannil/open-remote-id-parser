# Contributing to Open Remote ID Parser

Thank you for your interest in contributing to Open Remote ID Parser! This document provides guidelines and instructions for contributing.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Making Changes](#making-changes)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Issue Guidelines](#issue-guidelines)

## Code of Conduct

This project adheres to a code of conduct. By participating, you are expected to:

- Be respectful and inclusive
- Accept constructive criticism gracefully
- Focus on what is best for the community
- Show empathy towards other community members

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/open-remote-id-parser.git
   cd open-remote-id-parser
   ```
3. **Add the upstream remote**:
   ```bash
   git remote add upstream https://github.com/user/open-remote-id-parser.git
   ```

## Development Setup

### C++ Development

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get install cmake g++ git

# Install dependencies (macOS)
brew install cmake

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DORIP_BUILD_TESTS=ON
cmake --build . --parallel

# Run tests
ctest --output-on-failure
```

### Python Development

```bash
cd python
pip install -e ".[dev]"
pytest tests/
```

### Android Development

```bash
cd android
./gradlew :orip:build
```

## Making Changes

### Branch Naming

Use descriptive branch names:
- `feature/add-wifi-nan-support`
- `fix/bluetooth-parsing-crash`
- `docs/update-api-reference`
- `refactor/simplify-decoder-interface`

### Commit Messages

Follow the conventional commits format:

```
<type>(<scope>): <description>

[optional body]

[optional footer]
```

Types:
- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation changes
- `style`: Code style changes (formatting, etc.)
- `refactor`: Code refactoring
- `test`: Adding or updating tests
- `chore`: Maintenance tasks

Examples:
```
feat(decoder): add WiFi NAN support

Implement WiFi Neighbor Awareness Networking decoder
for parsing Remote ID broadcasts over WiFi NAN.

Closes #123
```

```
fix(android): resolve JNI memory leak

Free native resources properly in RemoteIDParser.close()
```

## Coding Standards

### C++

- Use C++17 features
- Follow the existing code style
- Use `snake_case` for functions and variables
- Use `PascalCase` for classes and types
- Use `SCREAMING_SNAKE_CASE` for constants
- Add comments for non-obvious logic
- Keep functions small and focused

```cpp
// Good
class RemoteIDParser {
public:
    ParseResult parse(const RawFrame& frame);

private:
    bool validate_payload(const std::vector<uint8_t>& payload);
};

// Constants
constexpr size_t MAX_PAYLOAD_SIZE = 256;
```

### Python

- Follow PEP 8 style guide
- Use type hints
- Use `snake_case` for functions and variables
- Use `PascalCase` for classes

```python
from typing import Optional

def parse_frame(payload: bytes, rssi: int) -> Optional[UAVObject]:
    """Parse a Remote ID frame."""
    pass
```

### Kotlin

- Follow Kotlin coding conventions
- Use data classes for DTOs
- Prefer immutability

```kotlin
data class UAVObject(
    val id: String,
    val location: LocationVector?,
    val rssi: Int
)
```

## Testing

### Writing Tests

- Write tests for all new features
- Write tests for bug fixes (to prevent regression)
- Aim for high code coverage
- Use descriptive test names

```cpp
TEST_F(ASTM_F3411_Test, DecodeBasicID_ValidSerialNumber) {
    auto msg = createBasicIDMessage("DJI1234567890ABC");
    auto adv = createBLEAdvertisement(msg);

    UAVObject uav;
    auto result = decoder.decode(adv, uav);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(uav.id, "DJI1234567890ABC");
}
```

### Running Tests

```bash
# C++ tests
cd build && ctest --output-on-failure

# Python tests
cd python && pytest -v

# With coverage
pytest --cov=orip tests/
```

## Submitting Changes

### Pull Request Process

1. **Update your fork**:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```

2. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature
   ```

3. **Make your changes** and commit them

4. **Push to your fork**:
   ```bash
   git push origin feature/your-feature
   ```

5. **Open a Pull Request** on GitHub

### PR Requirements

- [ ] All tests pass
- [ ] Code follows project style guidelines
- [ ] New features include tests
- [ ] Documentation is updated if needed
- [ ] Commit messages follow conventions
- [ ] PR description explains the changes

### Review Process

1. A maintainer will review your PR
2. Address any feedback
3. Once approved, your PR will be merged

## Issue Guidelines

### Bug Reports

Include:
- ORIP version
- Platform (OS, compiler, etc.)
- Steps to reproduce
- Expected vs actual behavior
- Sample code or data if possible

### Feature Requests

Include:
- Use case description
- Proposed solution (if any)
- Alternative solutions considered
- Willingness to implement

### Security Issues

For security vulnerabilities, please do NOT open a public issue. Instead, email the maintainers directly.

## Questions?

- Open a [Discussion](https://github.com/user/open-remote-id-parser/discussions) for questions
- Check existing issues before creating new ones
- Join our community chat (if available)

---

Thank you for contributing to Open Remote ID Parser!
