from PyQt5.QtWidgets import (
    QMainWindow,
    QApplication
)
import sys

class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.initUI()
    
    def initUI(self):
        self.setWindowTitle('Arknights Auto Machine')
        self.setGeometry(100, 100, 1200, 700)
        self.show()
    

def main():
    app = QApplication(sys.argv)
    window = MainWindow()
    sys.exit(app.exec_())

if __name__ == '__main__':
    main()
