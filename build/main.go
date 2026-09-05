package hardware_samsung

import (
	"android/soong/android"
	"android/soong/cc"
	"fmt"

	"github.com/google/blueprint/proptools"
)

func init() {
	android.PreDepsMutators(func(ctx android.RegisterMutatorsContext) {
		ctx.BottomUp("samsung_collect_prebuilt_shared_libs", collectPrebuiltLibs)
	})
	android.RegisterModuleType("samsung_cc_binary_with_prebuilts", func() android.Module {
		module := cc.BinaryFactory()
		module.AddProperties(&PrebuiltDepsProps{})
		return module
	})
}

type PrebuiltDepsProps struct {
	Prebuilt_shared_libs []string
}

func collectPrebuiltLibs(mctx android.BottomUpMutatorContext) {
	if mctx.ModuleType() == "samsung_cc_binary_with_prebuilts" {
		for _, prop := range mctx.Module().GetProperties() {
			if prebuiltDepsProps, ok := prop.(*PrebuiltDepsProps); ok {
				var depsToAdd []string

				for _, lib := range prebuiltDepsProps.Prebuilt_shared_libs {
					// find library in PRODUCT_SOONG_NAMESPACES
					for _, namespace := range mctx.Config().ProductVariables().NamespacesToExport {
						moduleName := fmt.Sprintf("//%s:%s", namespace, lib)
						if mctx.OtherModuleExists(moduleName) {
							depsToAdd = append(depsToAdd, moduleName)
						}
					}
				}
				proptools.AppendMatchingProperties(mctx.Module().GetProperties(), &struct {
					Shared_libs proptools.Configurable[[]string] `android:"arch_variant"`
				}{
					Shared_libs: proptools.NewSimpleConfigurable(depsToAdd),
				}, nil)
			}
		}
	}
}
