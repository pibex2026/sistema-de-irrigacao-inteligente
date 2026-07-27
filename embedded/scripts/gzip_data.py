import gzip
import shutil
from pathlib import Path

Import("env")

def compress_data_files(source, target, env):

    data_dir_str = env.get("PROJECT_DATA_DIR")
    if not data_dir_str:
        return

    data_dir = Path(data_dir_str)
    
    if not data_dir.exists():
        print(f"[GZIP] AVISO: Pasta 'data' não encontrada em: {data_dir}")
        return

    print(f"\n--- [GZIP] Compactando arquivos da pasta DATA ({data_dir}) ---")
    
    # Define as extensões aceitas em formato minúsculo
    extensions = {".js", ".html", ".css", ".ico", ".svg"}
    
    # Usa rglob para percorrer a pasta recursivamente de forma agnóstica
    for file_path in data_dir.rglob("*"):

        if file_path.name.endswith(".gz"):
            continue

        if file_path.is_file() and file_path.suffix.lower() in extensions:
            gz_path = file_path.with_name(f"{file_path.name}.gz")
            

            with open(file_path, 'rb') as f_in:
                with gzip.open(gz_path, 'wb', compresslevel=9) as f_out:
                    shutil.copyfileobj(f_in, f_out)
            
            print(f"  └─ Criado / Atualizado: {gz_path.name} (Original mantido)")
            
            # A linha 'file_path.unlink()' foi removida.
            # O seu arquivo fonte (ex: index.html) continuará intacto na pasta 'data/'!

    print("--- [GZIP] Concluído com sucesso! ---\n")


env.AddPreAction("$BUILD_DIR/littlefs.bin", compress_data_files)
env.AddPreAction("buildfs", compress_data_files)