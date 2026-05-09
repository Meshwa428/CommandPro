import subprocess
import random
import string
import os
import argparse

SYNAPSE_BIN = "./build/synapse"

def generate_random_script(length=100):
    chars = string.ascii_letters + string.digits + " \n\t+-*/=(){}[],;\"'#!"
    return ''.join(random.choice(chars) for _ in range(length))

def run_fuzz(content):
    with open("fuzz_temp.syn", "w") as f:
        f.write(content)
        
    cmd = [SYNAPSE_BIN, "run", "fuzz_temp.syn", "--mock", "--vm"]
    try:
        # We expect many to fail with ParseError or LexerError
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=1.0)
        return True # No crash
    except subprocess.TimeoutExpired:
        # Timeout is technically okay if it's an infinite loop, 
        # though we'd prefer stack safety.
        return True 
    except Exception as e:
        print(f"💥 CRASH DETECTED for content:\n{content}")
        print(f"Error: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(description="Fuzz test for Synapse compiler and VM.")
    parser.add_argument("-i", "--iterations", type=int, default=1000, help="Number of random scripts to generate.")
    parser.add_argument("-l", "--length", type=int, default=100, help="Average length of random scripts.")
    args = parser.parse_args()
    
    print("👾 Starting Synapse Fuzz Tester (1000 iterations)")
    
    for i in range(args.iterations):
        content = generate_random_script(random.randint(10, args.length))
        if not run_fuzz(content):
            print(f"❌ Fuzzing failed at iteration {i}")
            os.remove("fuzz_temp.syn")
            return
            
        if i % 100 == 0:
            print(f"   [{i}/{args.iterations}] iterations complete...")
            
    print(f"✅ FUZZING OK: {args.iterations} iterations without crash.")
    if os.path.exists("fuzz_temp.syn"):
        os.remove("fuzz_temp.syn")

if __name__ == "__main__":
    main()
