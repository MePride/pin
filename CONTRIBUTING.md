# Contributing to Pin

We welcome contributions to the Pin project! This document outlines how to contribute effectively.

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally
3. **Create a branch** for your feature or bug fix
4. **Make your changes** following our coding standards
5. **Test your changes** thoroughly
6. **Submit a pull request**

## Development Environment

### Prerequisites
- ESP-IDF v5.1 or later
- Python 3.7+
- Git

### Setup
```bash
git clone https://github.com/your-username/pin.git
cd pin/firmware
idf.py set-target esp32c3
idf.py menuconfig
```

## Coding Standards

### C Code (Firmware)
- Use C99 standard
- Function names: `pin_module_function_name`
- Constants: `PIN_MODULE_CONSTANT_NAME`
- Types: `pin_module_type_t`
- Include comprehensive error handling
- Document all public APIs

### JavaScript (PWA)
- Use ES6+ features where supported
- Follow standard formatting conventions
- Include JSDoc comments for functions
- Test across different browsers

## Testing

Run the system test suite before submitting:
```bash
python3 tools/test_system.py --ip <device_ip>
```

## Plugin Development

When contributing plugins:
- Follow the plugin API specifications
- Include comprehensive configuration options
- Test resource usage and limits
- Provide clear documentation

## Pull Request Process

1. Update documentation for any new features
2. Add tests for new functionality
3. Ensure all tests pass
4. Update CHANGELOG.md if applicable
5. Request review from maintainers

## Reporting Issues

When reporting bugs:
- Use the GitHub issue tracker
- Provide detailed reproduction steps
- Include system information and logs
- Use appropriate issue labels

## Code of Conduct

This project follows a code of conduct. By participating, you agree to uphold this code.

## Questions?

Feel free to open an issue for questions about contributing.