# Security Policy

## Supported Versions

We release patches for security vulnerabilities in the following versions:

| Version | Supported          |
| ------- | ------------------ |
| 1.x.x   | :white_check_mark: |
| < 1.0   | :x:                |

## Reporting a Vulnerability

The nx development team takes security bugs seriously. We appreciate your efforts to responsibly disclose your findings, and will make every effort to acknowledge your contributions.

### Where to Report

**DO NOT** report security vulnerabilities through public GitHub issues, discussions, or pull requests.

Instead, please send security reports to: **security@nx-notes.dev**

If you prefer encrypted communication, you can use our GPG key:
- Key ID: `0x1234567890ABCDEF`
- Fingerprint: `1234 5678 90AB CDEF 1234 5678 90AB CDEF 1234 5678`

### What to Include

To help us understand the nature and scope of the issue, please include as much of the following information as possible:

- **Type of issue** (e.g., buffer overflow, SQL injection, path traversal, etc.)
- **Full paths of source file(s)** related to the manifestation of the issue
- **The location of the affected source code** (tag/branch/commit or direct URL)
- **Any special configuration required** to reproduce the issue
- **Step-by-step instructions to reproduce** the issue
- **Proof-of-concept or exploit code** (if possible)
- **Impact of the issue**, including how an attacker might exploit it

### Response Timeline

We aim to respond to security reports within **48 hours** and will keep you updated on our progress. Here's what you can expect:

1. **Within 48 hours**: Acknowledgment of your report
2. **Within 1 week**: Initial assessment and triage
3. **Within 2 weeks**: Detailed investigation and fix development
4. **Within 4 weeks**: Release of patched version (for confirmed vulnerabilities)

### Disclosure Policy

- **Coordinated Disclosure**: We practice coordinated disclosure and ask that you do not publicly disclose the vulnerability until we have had a chance to address it
- **Public Credit**: With your permission, we will acknowledge your responsible disclosure in our security advisories and release notes
- **CVE Assignment**: For qualifying vulnerabilities, we will work with MITRE to assign a CVE identifier

## Security Features

nx is designed with security as a core principle. Our security features include:

### Data Protection
- **Local-first architecture**: Notes stored locally by default, with optional sync
- **Encryption at rest**: Support for per-file encryption using age/rage
- **Secure file operations**: Atomic writes with proper permissions (0600)
- **Path validation**: Canonicalization to prevent directory traversal attacks

### Process Security
- **Safe process execution**: No `system()` or `popen()` calls - uses `posix_spawn()` alternatives
- **Input validation**: Comprehensive validation in TUI editor and CLI commands
- **Memory safety**: Modern C++ practices with bounds checking and RAII
- **Exception safety**: Consistent error handling with `std::expected` pattern

### Development Security
- **Static analysis**: Automated security scans in CI/CD pipeline
- **Dependency scanning**: Regular vulnerability assessments of dependencies
- **Code review**: All changes require review before merging
- **Secure defaults**: Conservative default configurations

## Security Considerations

### User Responsibilities

While we strive to make nx secure by default, users should be aware of these security considerations:

#### File System Security
- **File permissions**: Ensure your notes directory has appropriate permissions (750 or 700)
- **Backup encryption**: If backing up to cloud storage, ensure backups are encrypted
- **Key management**: If using encryption, protect your keys appropriately

#### Network Security
- **Git sync**: When using Git synchronization, ensure remote repositories use HTTPS or SSH
- **AI integration**: Be aware that AI features may send note content to external services
- **Firewall**: Consider firewall rules if running nx in server environments

#### Operational Security
- **Regular updates**: Keep nx updated to the latest version for security patches
- **Audit logs**: Monitor system logs for unexpected nx activity
- **Access control**: Limit access to nx data files and configuration

### Known Security Limitations

We believe in transparency about our security limitations:

1. **AI Service Dependencies**: AI features require external API calls that may log queries
2. **Git Integration**: Git operations inherit security properties of the underlying Git installation
3. **System Dependencies**: Security depends on underlying OS and library security
4. **Terminal Security**: TUI mode may be vulnerable to terminal escape sequence attacks (mitigated)

## Security Architecture

### Defense in Depth

nx implements multiple layers of security:

1. **Input Layer**: Validation of all user inputs and file contents
2. **Processing Layer**: Safe memory management and bounds checking
3. **Storage Layer**: Secure file operations with proper permissions
4. **Transport Layer**: Encrypted communication for sync and AI features
5. **Audit Layer**: Logging of security-relevant operations

### Threat Model

We have considered the following threat scenarios:

#### High Priority Threats
- **Malicious note content**: Crafted files that exploit parsing vulnerabilities
- **Path traversal attacks**: Attempts to access files outside notes directory
- **Command injection**: Exploitation of external tool integrations
- **Memory corruption**: Buffer overflows or use-after-free vulnerabilities

#### Medium Priority Threats
- **Dependency vulnerabilities**: Security issues in third-party libraries
- **Configuration attacks**: Malicious configuration files
- **Race conditions**: Multi-threaded access to shared resources
- **Information disclosure**: Accidental exposure of sensitive data

#### Lower Priority Threats
- **Physical access**: Threats requiring local system access
- **Social engineering**: Attacks targeting user behavior
- **Side-channel attacks**: Timing or power analysis attacks

## Vulnerability Assessment

### Regular Security Audits

We conduct regular security assessments including:

- **Automated scanning**: Daily vulnerability scans of dependencies
- **Static analysis**: Code analysis for security issues in CI/CD
- **Dynamic testing**: Runtime security testing with sanitizers
- **Manual review**: Periodic manual security code reviews

### Security Testing

Our security testing includes:

- **Fuzzing**: Input fuzzing for file parsers and CLI interfaces
- **Penetration testing**: Simulated attacks on the application
- **Memory safety**: Testing with AddressSanitizer, MemorySanitizer, etc.
- **Regression testing**: Verification that fixed vulnerabilities stay fixed

## Security Contacts

- **Security Team**: security@nx-notes.dev
- **General Contact**: contact@nx-notes.dev
- **Bug Reports**: https://github.com/nx-org/nx/issues (for non-security issues only)

## Legal

### Safe Harbor

We consider security research conducted under this policy to be:
- Authorized in accordance with the Computer Fraud and Abuse Act (CFAA)
- Authorized in accordance with applicable state and local laws
- Exempt from Digital Millennium Copyright Act (DMCA) claims
- Protected from nx-initiated legal action if conducted in good faith

### Scope

This policy applies to:
- The nx application and all its components
- Official nx repositories on GitHub
- Official nx distribution packages
- Official nx documentation and websites

This policy does not apply to:
- Third-party integrations or plugins
- User-created content or configurations
- Systems or services outside of nx's direct control

## Recognition

We believe in recognizing the valuable contributions of security researchers. With your permission, we will:

- Acknowledge your contribution in our security advisories
- List your name/handle in our CONTRIBUTORS.md file
- Mention your contribution in relevant release notes
- Provide a written thank you letter upon request

## Updates

This security policy may be updated from time to time. The latest version will always be available at this location. Changes will be announced in our release notes and on our security mailing list.

---

**Last Updated**: 2025-08-17  
**Version**: 1.0.0