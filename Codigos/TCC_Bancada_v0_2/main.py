"""Aplicativo real por padrão; --simulacao mantém o demonstrador da versão 0.1."""
import sys

if __name__ == '__main__':
    if '--simulacao' in sys.argv:
        from bancada.gui import main
    else:
        from bancada.live_gui import main
    main()
