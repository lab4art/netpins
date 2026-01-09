Import("env")
import subprocess
import os

def get_git_info():
    try:
        # Get git commit hash (short)
        commit_hash = subprocess.check_output(
            ['git', 'rev-parse', '--short', 'HEAD'],
            cwd=env['PROJECT_DIR']
        ).decode('utf-8').strip()
        
        # Check if working directory is dirty
        status = subprocess.check_output(
            ['git', 'status', '--porcelain'],
            cwd=env['PROJECT_DIR']
        ).decode('utf-8').strip()
        
        dirty = '-dirty' if status else ''
        
        return commit_hash + dirty
    except Exception as e:
        print(f"Warning: Could not get git info: {e}")
        return "unknown"

git_version = get_git_info()
print(f"Building with git version: {git_version}")

# Add build flags with git info
env.Append(CPPDEFINES=[
    ("GIT_VERSION", f'\\"{git_version}\\"')
])
