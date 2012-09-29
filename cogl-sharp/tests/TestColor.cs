using Cogl;
using NUnit.Framework;

namespace Cogl.Test
{
    [TestFixture]
    public class TestColor : AssertionHelper
    {

        [SetUp]
        public void Init()
        {
            GlobalSettings.DefaultFloatingPointTolerance = 1e-6;
        }

        [Test]
        public void FloatConstructor()
        {
            Color color = new Color(0.1f, 0.2f, 0.3f, 0.4f);

            Expect(color.red, EqualTo(0.1f));
            Expect(color.green, EqualTo(0.2f));
            Expect(color.blue, EqualTo(0.3f));
            Expect(color.alpha, EqualTo(0.4f));
        }

        [Test]
        public void ByteConstructor()
        {
            Color color = new Color(10, 20, 30, 40);

            Expect(color.red, EqualTo(10.0f / 255.0f));
            Expect(color.green, EqualTo(20.0f / 255.0f));
            Expect(color.blue, EqualTo(30.0f / 255.0f));
            Expect(color.alpha, EqualTo(40.0f / 255.0f));
        }

        [Test]
        public void Premultiply()
        {
            Color color = new Color(1.0f, 1.0f, 1.0f, 0.5f);

            color.Premultiply();
            Expect(color.red, EqualTo(0.5f));
            Expect(color.green, EqualTo(0.5f));
            Expect(color.blue, EqualTo(0.5f));
            Expect(color.alpha, EqualTo(0.5f));

            color.UnPremultiply();
            Expect(color.red, EqualTo(1.0f));
            Expect(color.green, EqualTo(1.0f));
            Expect(color.blue, EqualTo(1.0f));
            Expect(color.alpha, EqualTo(0.5f));
        }
    }
}
