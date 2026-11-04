import importlib.util
from pathlib import Path
import tempfile
import unittest

spec = importlib.util.spec_from_file_location("gamepads", Path(__file__).parents[1] / "sdk/cmake/arkos-gamepads.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class MappingTests(unittest.TestCase):
    def test_physical_labels_axes_hat_and_hotkey(self):
        xml = """<inputList><inputConfig type="keyboard" deviceGUID="-1"/>
<inputConfig type="joystick" deviceGUID="19000000010000000100000001010000" deviceName="R36S,test">
<input name="a" type="button" id="0" value="1"/>
<input name="b" type="button" id="1" value="1"/>
<input name="select" type="button" id="12" value="1"/>
<input name="hotkeyenable" type="button" id="12" value="1"/>
<input name="up" type="hat" id="0" value="1"/>
<input name="left" type="axis" id="4" value="-1"/>
<input name="leftanalogup" type="axis" id="1" value="-1"/>
<input name="rightanalogleft" type="axis" id="2" value="1"/>
</inputConfig></inputList>"""
        with tempfile.TemporaryDirectory() as directory:
            file = Path(directory) / "es_input.cfg"
            file.write_text(xml)
            mappings = list(module.convert(file))
        self.assertEqual(mappings, ["19000000010000000100000001010000,R36S test,b:b0,a:b1,back:b12,dpup:h0.1,dpleft:-a4,lefty:a1,rightx:a2~,platform:Linux,"])


if __name__ == "__main__":
    unittest.main()
