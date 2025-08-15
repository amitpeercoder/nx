#include "nx/cli/commands/encrypt_command.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

#include "nx/crypto/age_crypto.hpp"
#include "nx/core/note_id.hpp"

namespace nx::cli {

EncryptCommand::EncryptCommand(Application& app) : app_(app) {}

void EncryptCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "Note ID to encrypt/decrypt (or 'all' for all notes)")
     ->required();
  
  cmd->add_flag("--decrypt,-d", decrypt_, "Decrypt instead of encrypt");
  cmd->add_flag("--all", all_notes_, "Apply to all notes");
  cmd->add_option("--key-file,-k", key_file_, "Path to age key file (defaults to config)");
  cmd->add_flag("--generate-key,-g", generate_key_, "Generate a new encryption key");
  cmd->add_option("--output-key,-o", output_key_file_, "Output path for generated key");
}

Result<int> EncryptCommand::execute(const GlobalOptions& options) {
  try {
    // Handle key generation
    if (generate_key_) {
      std::string key_output = output_key_file_.empty() ? 
        (app_.config().data_dir / "encryption.key").string() : output_key_file_;
      
      auto key_result = nx::crypto::AgeCrypto::generateKeyPair(key_output);
      if (!key_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << key_result.error().message() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << key_result.error().message() << std::endl;
        }
        return 1;
      }
      
      if (options.json) {
        nlohmann::json result;
        result["success"] = true;
        result["key_file"] = key_output;
        result["public_key"] = key_result->public_key;
        result["fingerprint"] = key_result->fingerprint;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Generated encryption key: " << key_output << std::endl;
        std::cout << "Public key: " << key_result->public_key << std::endl;
        std::cout << "Fingerprint: " << key_result->fingerprint << std::endl;
        std::cout << "\nIMPORTANT: Store this key file securely and back it up!" << std::endl;
        std::cout << "Without it, you will not be able to decrypt your notes." << std::endl;
      }
      
      return 0;
    }
    
    // Determine key file to use
    std::string key_path = key_file_.empty() ? 
      (app_.config().data_dir / "encryption.key").string() : key_file_;
    
    // Check if encryption is available
    if (!nx::crypto::AgeCrypto::isAvailable()) {
      if (options.json) {
        std::cout << R"({"error": "Age encryption tools not available", "success": false})" << std::endl;
      } else {
        std::cout << "Error: Age encryption tools not available." << std::endl;
        std::cout << "Please install age/rage: https://github.com/FiloSottile/age" << std::endl;
      }
      return 1;
    }
    
    // Initialize encryption
    auto crypto_result = nx::crypto::AgeCrypto::initialize(key_path);
    if (!crypto_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << crypto_result.error().message() << R"(", "success": false})" << std::endl;
      } else {
        std::cout << "Error: " << crypto_result.error().message() << std::endl;
        if (crypto_result.error().code() == ErrorCode::kFileError) {
          std::cout << "Hint: Generate a key with: nx encrypt --generate-key" << std::endl;
        }
      }
      return 1;
    }
    
    auto& crypto = crypto_result.value();
    
    // Handle all notes
    if (all_notes_ || note_id_ == "all") {
      auto notes_result = app_.noteStore().list();
      if (!notes_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << notes_result.error().message() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << notes_result.error().message() << std::endl;
        }
        return 1;
      }
      
      int processed = 0;
      int failed = 0;
      
      for (const auto& note_id : notes_result.value()) {
        auto note_result = app_.noteStore().load(note_id);
        if (!note_result.has_value()) {
          failed++;
          if (!options.quiet) {
            std::cerr << "Warning: Failed to load note " << note_id.toString() 
                      << ": " << note_result.error().message() << std::endl;
          }
          continue;
        }
        
        auto& note = note_result.value();
        
        try {
          if (decrypt_) {
            // Try to decrypt
            auto decrypted_result = crypto.decrypt(note.content(), note.id());
            if (decrypted_result.has_value()) {
              // Update note with decrypted content
              note.setContent(decrypted_result.value());
              auto store_result = app_.noteStore().store(note);
              if (store_result.has_value()) {
                processed++;
              } else {
                failed++;
                if (!options.quiet) {
                  std::cerr << "Warning: Failed to store decrypted note " << note.id().toString() 
                           << ": " << store_result.error().message() << std::endl;
                }
              }
            }
          } else {
            // Try to encrypt (skip if already encrypted)
            if (note.content().find("-----BEGIN AGE ENCRYPTED FILE-----") == std::string::npos) {
              auto encrypted_result = crypto.encrypt(note.content(), note.id());
              if (encrypted_result.has_value()) {
                // Update note with encrypted content
                note.setContent(encrypted_result.value());
                auto store_result = app_.noteStore().store(note);
                if (store_result.has_value()) {
                  processed++;
                } else {
                  failed++;
                  if (!options.quiet) {
                    std::cerr << "Warning: Failed to store encrypted note " << note.id().toString() 
                             << ": " << store_result.error().message() << std::endl;
                  }
                }
              } else {
                failed++;
                if (!options.quiet) {
                  std::cerr << "Warning: Failed to encrypt note " << note.id().toString() 
                           << ": " << encrypted_result.error().message() << std::endl;
                }
              }
            }
          }
        } catch (const std::exception& e) {
          failed++;
          if (!options.quiet) {
            std::cerr << "Warning: Error processing note " << note.id().toString() 
                     << ": " << e.what() << std::endl;
          }
        }
      }
      
      if (options.json) {
        nlohmann::json result;
        result["success"] = failed == 0;
        result["processed"] = processed;
        result["failed"] = failed;
        result["operation"] = decrypt_ ? "decrypt" : "encrypt";
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << (decrypt_ ? "Decrypted" : "Encrypted") << " " << processed << " notes";
        if (failed > 0) {
          std::cout << " (" << failed << " failed)";
        }
        std::cout << std::endl;
      }
      
      return failed == 0 ? 0 : 1;
    }
    
    // Handle single note
    auto resolved_id = app_.noteStore().resolveSingle(note_id_);
    if (!resolved_id.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << resolved_id.error().message() << R"(", "note_id": ")" 
                  << note_id_ << R"(", "success": false})" << std::endl;
      } else {
        std::cout << "Error: " << resolved_id.error().message() << std::endl;
      }
      return 1;
    }
    
    auto note_result = app_.noteStore().load(resolved_id.value());
    if (!note_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << note_result.error().message() << R"(", "note_id": ")" 
                  << resolved_id->toString() << R"(", "success": false})" << std::endl;
      } else {
        std::cout << "Error: " << note_result.error().message() << std::endl;
      }
      return 1;
    }
    
    auto note = note_result.value();
    
    if (decrypt_) {
      // Decrypt note
      auto decrypted_result = crypto.decrypt(note.content(), note.id());
      if (!decrypted_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << decrypted_result.error().message() << R"(", "note_id": ")" 
                    << note.id().toString() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << decrypted_result.error().message() << std::endl;
        }
        return 1;
      }
      
      note.setContent(decrypted_result.value());
    } else {
      // Encrypt note (check if already encrypted)
      if (note.content().find("-----BEGIN AGE ENCRYPTED FILE-----") != std::string::npos) {
        if (options.json) {
          std::cout << R"({"error": "Note is already encrypted", "note_id": ")" 
                    << note.id().toString() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: Note is already encrypted" << std::endl;
        }
        return 1;
      }
      
      auto encrypted_result = crypto.encrypt(note.content(), note.id());
      if (!encrypted_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << encrypted_result.error().message() << R"(", "note_id": ")" 
                    << note.id().toString() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << encrypted_result.error().message() << std::endl;
        }
        return 1;
      }
      
      note.setContent(encrypted_result.value());
    }
    
    // Store updated note
    auto store_result = app_.noteStore().store(note);
    if (!store_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << store_result.error().message() << R"(", "note_id": ")" 
                  << note.id().toString() << R"(", "success": false})" << std::endl;
      } else {
        std::cout << "Error: " << store_result.error().message() << std::endl;
      }
      return 1;
    }
    
    // Update search index
    auto index_result = app_.searchIndex().updateNote(note);
    if (!index_result.has_value() && !options.quiet) {
      std::cerr << "Warning: Failed to update search index: " << index_result.error().message() << std::endl;
    }
    
    if (options.json) {
      nlohmann::json result;
      result["success"] = true;
      result["note_id"] = note.id().toString();
      result["operation"] = decrypt_ ? "decrypt" : "encrypt";
      std::cout << result.dump(2) << std::endl;
    } else {
      std::cout << (decrypt_ ? "Decrypted" : "Encrypted") << " note: " << note.id().toString() << std::endl;
    }
    
    return 0;
    
  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"(", "success": false})" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

} // namespace nx::cli