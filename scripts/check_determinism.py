import subprocess
import hashlib
import sys

def get_output_hash(cmd):
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        return f"ERROR_{result.returncode}_{result.stderr.strip()}"
    return hashlib.sha256(result.stdout.encode()).hexdigest()

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 check_determinism.py <script_path>")
        sys.exit(1)
        
    script = sys.argv[1]
    cmd = ["./build/synapse", "run", script, "--mock", "--vm"]
    
    print(f"🧐 Checking determinism for: {script}")
    
    first_hash = get_output_hash(cmd)
    
    for i in range(1, 100):
        current_hash = get_output_hash(cmd)
        if current_hash != first_hash:
            print(f"❌ DETERMINISM FAILURE at run {i}!")
            print(f"Original Hash: {first_hash}")
            print(f"Current Hash:  {current_hash}")
            sys.exit(1)
            
    print(f"✅ DETERMINISM OK: 100/100 runs matched (Hash: {first_hash[:16]}...)")

if __name__ == "__main__":
    main()
