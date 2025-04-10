import os

def list_directory_contents():
    # Get directory path from user
    dir_path = input("Enter the directory path (or 'q' to quit): ")
    
    if dir_path.lower() == 'q':
        return False
        
    try:
        # List all files and directories in the given path
        contents = os.listdir(dir_path)
        
        # Print each item
        print(f"\nContents of {dir_path}:")
        for item in contents:
            print(f"- {item}")
            
    except FileNotFoundError:
        print(f"Error: Directory '{dir_path}' not found")
    except PermissionError:
        print(f"Error: Permission denied to access '{dir_path}'")
    except Exception as e:
        print(f"Error: {str(e)}")
    return True

if __name__ == "__main__":
    while list_directory_contents():
        pass
    print("Goodbye!")
