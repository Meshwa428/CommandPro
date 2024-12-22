import os
from pathlib import Path

def get_unique_uppercase_words():
    # Path to the directory containing .csc files
    directory = Path(r"P:\Python\CommandPro\lang-test")
    
    # Set to store unique uppercase words
    uppercase_words = set()
    
    # Read all .csc files in the directory
    for file_path in directory.glob("*.csc"):
        try:
            with open(file_path, 'r', encoding='utf-8') as file:
                content = file.read()
                # Replace delimiters with spaces
                for delimiter in [';', '(', ')', '{', '}']:
                    content = content.replace(delimiter, ' ')
                # Split content into words and check each word
                words = content.split()
                for word in words:
                    # Check if word is all uppercase
                    if word.isupper():
                        uppercase_words.add(word)
        except Exception as e:
            print(f"Error reading {file_path}: {e}")
    
    # Print all unique uppercase words
    for word in sorted(uppercase_words):
        print(word)

if __name__ == "__main__":
    get_unique_uppercase_words()
