using Cogl;
using NUnit.Framework;

namespace Cogl.Test
{
    [TestFixture]
    public class TestMatrix : AssertionHelper
    {

        [SetUp]
        public void Init()
        {
            GlobalSettings.DefaultFloatingPointTolerance = 1e-6;
        }

        [Test]
        public void InitIdentity()
        {
            Matrix i;

            i.InitIdentity();

            Expect(i.xx, EqualTo(1.0f));
            Expect(i.yx, EqualTo(0.0f));
            Expect(i.zx, EqualTo(0.0f));
            Expect(i.wx, EqualTo(0.0f));

            Expect(i.xy, EqualTo(0.0f));
            Expect(i.yy, EqualTo(1.0f));
            Expect(i.zy, EqualTo(0.0f));
            Expect(i.wy, EqualTo(0.0f));

            Expect(i.xz, EqualTo(0.0f));
            Expect(i.yz, EqualTo(0.0f));
            Expect(i.zz, EqualTo(1.0f));
            Expect(i.wz, EqualTo(0.0f));

            Expect(i.xw, EqualTo(0.0f));
            Expect(i.yw, EqualTo(0.0f));
            Expect(i.zw, EqualTo(0.0f));
            Expect(i.ww, EqualTo(1.0f));
        }
    }
}
