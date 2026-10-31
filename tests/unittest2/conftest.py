# Đảm bảo tests/unittest2 nằm trên sys.path để import _harness bất kể nơi chạy unittest2.
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))